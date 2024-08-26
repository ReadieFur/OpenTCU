#include <Arduino.h>
#include <iostream>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <cstring>
#include <freertos/task.h>
#include <freertos/queue.h>

// #define ENABLE_EMULATION_TASK 20.0f

#define REED_POSITIVE GPIO_NUM_0
#define REED_NEGATIVE GPIO_NUM_1
#define RELAY_BASE GPIO_NUM_2

#define SETTINGS_API_TEMPLATE "wheelCircumference:%u,enabled:%d,speedMultiplier:%f,enableMultiplierAtSpeed:%lf,timeBetweenUpdates:%u,revolutionsBetweenUpdates:%u,disableMultiplierAtSpeed:%lf,logUpdateInterval:%u,maxPulseDurationBuffer:%u"
#define STATS_API_TEMPLATE "realRpm:%lu,realKph:%lf,fakedRpm:%lu,fakedKph:%lf,updateLatency:%li"

#define NAMEOF(name) #name

Preferences preferences;
AsyncWebServer server(80);
struct SSettings
{
	uint wheelCircumference; //In millimeters.
	bool enabled; //Wether or not to enable the speed multiplier at all.
	float_t speedMultiplier; //How much to raise the top speed by.
	double_t enableMultiplierAtSpeed; //In km/h.
	// uint pulseDuration; //Time to pulse for in ms.
	uint timeBetweenUpdates; //Time between speed calculation updates, used for low speed.
	uint revolutionsBetweenUpdates; //Time between speed calculation updates, used for high speed.
	double_t disableMultiplierAtSpeed;
	uint logUpdateInterval; //The time between log updates in milliseconds.
	uint maxPulseDurationBuffer; //The maximum amount of time in milliseconds that a wheel pulse can last before being considered too long (in milliseconds).
} volatile settings = {
	.wheelCircumference = 2145,
	.enabled = true,
	.speedMultiplier = 1.5f,
	.enableMultiplierAtSpeed = 18.0,
	// .pulseDuration = 10,
	.timeBetweenUpdates = 3000,
	.revolutionsBetweenUpdates = 5,
	.disableMultiplierAtSpeed = 40,
	.logUpdateInterval = 1000,
	.maxPulseDurationBuffer = 500
};

//https://electronics.stackexchange.com/questions/89/how-do-i-measure-the-rpm-of-a-wheel
volatile uint reedPulses = 0;
volatile int lastReedState = LOW;
volatile QueueHandle_t pulseDurations;
volatile ulong pulseDurationBuffer = 0;
volatile bool modifiersEnabled = false;
volatile bool pauseInterrupt = false;
ulong lastRPMUpdate = 0;
ulong averagePulseDuration = 0;
ulong realRpm = 0;
double_t realKph = 0;
double_t fakedKph = 0;
ulong fakedRpm = 0;
ulong updateLatency = 0;
ulong lastOtaLog = 0;

//This is getting a little too big for my liking of an ISR but it is probably file.
void IRAM_ATTR ReedISR()
{
	int reedState = digitalRead(REED_NEGATIVE);

	//If modifiers are currently disabled, bypass all later calculations for reduced update latency.
	if (!modifiersEnabled)
		digitalWrite(RELAY_BASE, reedState);

	if (lastReedState == LOW && reedState == HIGH)
	{
		pulseDurationBuffer = millis();
	}
	else if (!pauseInterrupt && lastReedState == HIGH && reedState == LOW)
	{
		//Increment counters on falling edge unless we are processing the interrupt data in the ReedTask.

		//Add protection to this value from being too massive when the wheel is stopped.
		if (pulseDurationBuffer > settings.maxPulseDurationBuffer)
			pulseDurationBuffer = settings.maxPulseDurationBuffer;
		ulong pulseDuration = millis() - pulseDurationBuffer;

		while (xQueueSendFromISR(pulseDurations, &pulseDuration, NULL) == errQUEUE_FULL)
			xQueueReceiveFromISR(pulseDurations, &pulseDuration, NULL);
		reedPulses++;
	}

	lastReedState = reedState;
}

void ReedTask(void* args)
{
	while (true)
	{
		ulong now = millis();
		ulong timeBetweenUpdates = now - lastRPMUpdate;

		//Update RPM every x revolutions or x milliseconds, whichever comes first (revolutions will occur at higher speeds and milliseconds will occur at lower speeds).
		if (reedPulses >= settings.revolutionsBetweenUpdates || timeBetweenUpdates >= settings.timeBetweenUpdates)
		{
			////Disable the interrupt while calculating the RPM to avoid incorrect values.
			//Don't disable the interrupt and instead set a boolean value so that if we have direct relay enabled we don't miss any signals.
			// detachInterrupt(REED_NEGATIVE);
			pauseInterrupt = true;

			//Set this here, before we update this check as it will take at least one cycle to update.
			if (modifiersEnabled)
				updateLatency = timeBetweenUpdates;
			else
				updateLatency = 0; //Not actually 0 but the interrupt time is so fast it is basically 0.

			ulong localAveragePulseDuration = 0;
			ulong pulseDuration;
			while (xQueueReceive(pulseDurations, &pulseDuration, 0) == pdTRUE)
				localAveragePulseDuration += pulseDuration;
			averagePulseDuration = localAveragePulseDuration / reedPulses;

			//Calculate the RPM.
			//The division of 60000 is to convert from milliseconds to minutes.
			realRpm = reedPulses * (60000.0 / timeBetweenUpdates);

			//Calculate the real wheel speed. The wheel circumference is needed in meters for this calculation so divide by 1000.
			//This then gives us meters per minute, to convert it to KM/H we multiply by 0.06.
			realKph = realRpm * (settings.wheelCircumference / 1000) * 0.06;

			modifiersEnabled = settings.enabled && realKph >= settings.enableMultiplierAtSpeed && realKph <= settings.disableMultiplierAtSpeed;

			//Reset counter and update last interval.
			reedPulses = 0;
			xQueueReset(pulseDurations);
			lastRPMUpdate = now;

			//Resume ISR.
			// attachInterrupt(REED_NEGATIVE, ReedISR, CHANGE);
			pauseInterrupt = false;
		}

		vTaskDelay(pdMS_TO_TICKS(1));
	}
}

void RelayTask(void* args)
{
	while (true)
	{
		//If we are idle then don't relay any speed information.
		//On this chip if we ignored this, the calculations would underflow and cause the delay to be uint.max.
		if (!settings.enabled || !modifiersEnabled || realKph <= 0)
		{
			vTaskDelay(pdMS_TO_TICKS(1));
			continue;
		}

		//Check if we should enable the speed multiplier.
		fakedKph = realKph;
		ulong pulseDuration = averagePulseDuration;
		if (realKph >= settings.enableMultiplierAtSpeed)
		{
			fakedKph /= settings.speedMultiplier;
			pulseDuration /= settings.speedMultiplier;

			//Simulate X kph wheel speed.
			float_t metersPerMinute = (fakedKph * 1000) / 60;
			fakedRpm = metersPerMinute / (settings.wheelCircumference / 1000);
		}
		else
		{
			fakedRpm = realRpm;
		}

		float_t pulsesPerSecond = fakedRpm / 60.0f;
		uint delay = (1000.0f / pulsesPerSecond) - pulseDuration;

		digitalWrite(RELAY_BASE, HIGH);
		vTaskDelay(pdMS_TO_TICKS(pulseDuration));
		digitalWrite(RELAY_BASE, LOW);

		vTaskDelay(pdMS_TO_TICKS(delay));
	}
}

void LoggingTask(void* args)
{
	while (true)
	{
		std::cout
			<< "Real rpm: " << realRpm << "\n"
			<< "Real km/h: " << realKph << "\n";

		if (modifiersEnabled)
		{
			std::cout
				<< "Faked rpm: " << fakedRpm << "\n"
				<< "Faked km/h: " << fakedKph << "\n";
		}
		else
		{
			std::cout << "Modifiers disabled";

			if (realKph < settings.enableMultiplierAtSpeed)
				std::cout << " (Below activation speed of " << settings.enableMultiplierAtSpeed << "km/h)";
			else if (realKph > settings.disableMultiplierAtSpeed)
				std::cout << " (Above deactivation speed of " << settings.disableMultiplierAtSpeed << "km/h)";

			std::cout << "\n";
		}

		std::cout << "Update latency (ms): " << updateLatency << "\n";

		std::cout << std::flush;

		vTaskDelay(pdMS_TO_TICKS(settings.logUpdateInterval));
	}
}

void EmulationTask(void* args)
{
	while (true)
	{
		//Simulate X kph wheel speed.
		float_t desiredRealKph = ENABLE_EMULATION_TASK;
		
		float_t metersPerMinute = (desiredRealKph * 1000) / 60;
		float_t rpm = metersPerMinute / (settings.wheelCircumference / 1000);
		float_t pulsesPerSecond = rpm / 60;
		uint pulseDuration = 10;
		int delay = (1000 / pulsesPerSecond) - pulseDuration;
		if (delay < 0)
			delay = 0;

		#if true
		reedPulses++;
		#else
		//This unfortunately doesn't work in my current configuration.
		digitalWrite(REED_NEGATIVE, HIGH);
		vTaskDelay(pdMS_TO_TICKS(pulseDuration));
		digitalWrite(REED_NEGATIVE, LOW);
		#endif

		vTaskDelay(pdMS_TO_TICKS(delay));
	}
}

// void LoopTask(void* args)
// {
// 	while (true)
// 	{
// 		ElegantOTA.loop();
// 	}
// }

void OnGet_Settings(AsyncWebServerRequest* request)
{
	char buf[512];
	sprintf(buf,
		SETTINGS_API_TEMPLATE,
		settings.wheelCircumference,
		settings.enabled,
		settings.speedMultiplier,
		settings.enableMultiplierAtSpeed,
		// settings.pulseDuration,
		settings.timeBetweenUpdates,
		settings.revolutionsBetweenUpdates,
		settings.disableMultiplierAtSpeed,
		settings.logUpdateInterval,
		settings.maxPulseDurationBuffer
	);
	request->send(200, "text/plain", buf);
}

void OnPost_Settings(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total)
{
	if (!index)
	{
		request->_tempObject = malloc(total + 1);
		if (request->_tempObject == NULL)
		{
            request->send(500, "text/plain", "Server error: Memory allocation failed.");
            return;
        }
	}

	//Copy the current chunk of data into the allocated buffer.
	memcpy(static_cast<uint8_t*>(request->_tempObject) + index, data, len);

	if (index + len == total)
	{
		//Null-terminate the buffer.
        static_cast<uint8_t*>(request->_tempObject)[total] = '\0';

		SSettings tmpSettings = {};

		//Extract values using sscanf.
		int result = sscanf(static_cast<char*>(request->_tempObject),
			SETTINGS_API_TEMPLATE,
			&tmpSettings.wheelCircumference,
			&tmpSettings.enabled,
			&tmpSettings.speedMultiplier,
			&tmpSettings.enableMultiplierAtSpeed,
			// &tmpSettings.pulseDuration,
			&tmpSettings.timeBetweenUpdates,
			&tmpSettings.revolutionsBetweenUpdates,
			&tmpSettings.disableMultiplierAtSpeed,
			&tmpSettings.logUpdateInterval,
			&tmpSettings.maxPulseDurationBuffer
		);

		free(request->_tempObject);

		if (result != 4)
		{
        	request->send(400, "text/plain", "Invalid request.");
			return;
		}

		//I can't directly assign tmpSettings to settings here as settings is marked as volatile.
		settings.wheelCircumference = tmpSettings.wheelCircumference;
		settings.enabled = tmpSettings.enabled;
		settings.speedMultiplier = tmpSettings.speedMultiplier;
		settings.enableMultiplierAtSpeed = tmpSettings.enableMultiplierAtSpeed;
		// settings.pulseDuration = tmpSettings.pulseDuration;
		settings.timeBetweenUpdates = tmpSettings.timeBetweenUpdates;
		settings.revolutionsBetweenUpdates = tmpSettings.revolutionsBetweenUpdates;
		settings.disableMultiplierAtSpeed = tmpSettings.disableMultiplierAtSpeed;
		settings.logUpdateInterval = tmpSettings.logUpdateInterval;
		settings.maxPulseDurationBuffer = tmpSettings.maxPulseDurationBuffer;

        request->send(200, "text/plain", "Settings received and parsed.");
	}
}

void OnGet_Stats(AsyncWebServerRequest* request)
{
	char buf[256];
	sprintf(buf,
		STATS_API_TEMPLATE,
		realRpm,
		realKph,
		fakedRpm,
		fakedKph,
		updateLatency
	);
	request->send(200, "text/plain", buf);
}

void OnPost_Reset(AsyncWebServerRequest* request)
{
	request->send(202);
	preferences.clear();
	ESP.restart();
}

void OnPost_Restart(AsyncWebServerRequest* request)
{
	request->send(202);
	ESP.restart();
}

void OnOtaStart()
{
	std::cout << "OTA update started." << std::endl;
}

void OnOtaProgress(size_t current, size_t final)
{
	//Log at most every 1 seconds.
	if (millis() - lastOtaLog > 1000)
	{
		lastOtaLog = millis();
		std::cout << "OTA Progress: " << current << "/" << final << " bytes" << std::endl;
	}
}

void OnOtaEnd(bool success)
{
	//Log when OTA has finished.
	if (success)
	{
		std::cout << "Rebooting to install OTA update..." << std::endl;
		// vTaskDelay(pdMS_TO_TICKS(2000));
		ESP.restart();
	}
	else
	{
		std::cout << "Error during OTA update!" << std::endl;
	}
}

void setup()
{
	Serial.begin(115200);

	pinMode(REED_POSITIVE, OUTPUT);
	pinMode(REED_NEGATIVE, INPUT_PULLDOWN);
	pinMode(RELAY_BASE, OUTPUT);
	pinMode(GPIO_NUM_3, INPUT_PULLDOWN);
	digitalWrite(REED_POSITIVE, HIGH);

	preferences.begin("reed");

	#ifdef ENABLE_EMULATION_TASK
	preferences.clear();
	#endif

	if (preferences.isKey("wc"))
		settings.wheelCircumference = preferences.getUInt("wc", settings.wheelCircumference);
	else
		preferences.putUInt("wc", settings.wheelCircumference);
	if (preferences.isKey("en"))
		settings.enabled = preferences.getBool("en", settings.enabled);
	else
		preferences.putBool("en", settings.enabled);
	if (preferences.isKey("sm"))
		settings.speedMultiplier = preferences.getFloat("sm", settings.speedMultiplier);
	else
		preferences.putFloat("sm", settings.speedMultiplier);
	if (preferences.isKey("ens"))
		settings.enableMultiplierAtSpeed = preferences.putDouble("ens", settings.enableMultiplierAtSpeed);
	else
		preferences.getDouble("ens", settings.enableMultiplierAtSpeed);
	// if (preferences.isKey("pd"))
	// 	settings.pulseDuration = preferences.putUInt("pd", settings.pulseDuration);
	// else
	// 	preferences.getUInt("pd", settings.pulseDuration);
	if (preferences.isKey("tbu"))
		settings.timeBetweenUpdates = preferences.putUInt("tbu", settings.timeBetweenUpdates);
	else
		preferences.getUInt("tbu", settings.timeBetweenUpdates);
	if (preferences.isKey("rbu"))
		settings.revolutionsBetweenUpdates = preferences.putUInt("rbu", settings.revolutionsBetweenUpdates);
	else
		preferences.getUInt("rbu", settings.revolutionsBetweenUpdates);

	pulseDurations = xQueueCreate(settings.revolutionsBetweenUpdates, sizeof(ulong));

	WiFi.mode(WIFI_AP);
	WiFi.softAP("ESP-REED", NULL, 1 /*Don't care about this value (default used)*/, 1 /*Need to set so SSID is hidden*/);
	//https://github.com/esphome/issues/issues/4893
	WiFi.setTxPower(WIFI_POWER_8_5dBm);

	SPIFFS.begin(true);

	server.begin();

	ElegantOTA.begin(&server);
	ElegantOTA.onStart(OnOtaStart);
	ElegantOTA.onProgress(OnOtaProgress);
	ElegantOTA.onEnd(OnOtaEnd);

	server.on("/", HTTP_GET, [](AsyncWebServerRequest* request){ request->send(SPIFFS, "/index.html", "text/html"); });
	server.on("/settings", HTTP_GET, OnGet_Settings);
	server.on("/settings", HTTP_POST, NULL, NULL, OnPost_Settings);
	server.on("/stats", HTTP_GET, OnGet_Stats);
	server.on("/reset", HTTP_POST, OnPost_Reset);
	server.on("/restart", HTTP_POST, OnPost_Restart);
	// //https://arduino.stackexchange.com/questions/89688/generalize-webserver-routing
	// server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

	//Create my own loop task as the default task has too high of a priority and prevents other secondary tasks from running.
	attachInterrupt(REED_NEGATIVE, ReedISR, CHANGE);
	xTaskCreate(ReedTask, NAMEOF(ReedTask), configMINIMAL_STACK_SIZE + 2048, NULL, (UBaseType_t)(configMAX_PRIORITIES * 0.4f), NULL);
	xTaskCreate(RelayTask, NAMEOF(RelayTask), configMINIMAL_STACK_SIZE + 2048, NULL, (UBaseType_t)(configMAX_PRIORITIES * 0.5f), NULL);
	xTaskCreate(LoggingTask, NAMEOF(LoggingTask), configMINIMAL_STACK_SIZE + 4096, NULL, (UBaseType_t)(configMAX_PRIORITIES * 0.2f), NULL);
	// xTaskCreate(LoopTask, NAMEOF(LoopTask), configMINIMAL_STACK_SIZE + 2048, NULL, (UBaseType_t)(configMAX_PRIORITIES * 0.1f), NULL);
	#ifdef ENABLE_EMULATION_TASK
	xTaskCreate(EmulationTask, NAMEOF(EmulationTask), configMINIMAL_STACK_SIZE + 512, NULL, (UBaseType_t)(configMAX_PRIORITIES * 0.3f), NULL);
	#endif
}

void loop()
{
	vTaskDelete(NULL);
}
