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

#define REED_POSITIVE GPIO_NUM_0
#define REED_NEGATIVE GPIO_NUM_1
#define RELAY_BASE GPIO_NUM_2

#define SETTINGS_API_TEMPLATE "wheelCircumference:%u,enabled:%d,speedMultiplier:%f,enableMultiplierAtSpeed:%lf,pulseDuration:%u,timeBetweenUpdates:%u,revolutionsBetweenUpdates:%u"
#define STATS_API_TEMPLATE "realRpm:%lu,realKph:%lf,fakedRpm:%lu,fakedKph:%lf,updateLatency:%li"

#define NAMEOF(name) #name

Preferences preferences;
AsyncWebServer server(80);
struct SSettings
{
	uint wheelCircumference; //In millimeters.
	bool enabled; //Wether or not to enable the speed multiplier at all. //TODO: Have this skip the relay task all-together when false and send the pulses directly to the relay base.
	float_t speedMultiplier; //How much to raise the top speed by.
	double_t enableMultiplierAtSpeed; //In mm/s.
	uint pulseDuration; //Time to pulse for in ms.
	uint timeBetweenUpdates; //Time between speed calculation updates, used for low speed.
	uint revolutionsBetweenUpdates; //Time between speed calculation updates, used for high speed.
} settings = {
	.wheelCircumference = 2145,
	.enabled = true,
	.speedMultiplier = 1.5f,
	.enableMultiplierAtSpeed = 20.0,
	.pulseDuration = 10,
	.timeBetweenUpdates = 3000,
	.revolutionsBetweenUpdates = 20
};

//https://electronics.stackexchange.com/questions/89/how-do-i-measure-the-rpm-of-a-wheel
volatile uint reedPulses = 0;
ulong lastRPMUpdate = 0;
ulong realRpm = 0;
double_t realKph = 0;
double_t fakedKph = 0;
ulong fakedRpm = 0;
long updateLatency = -1;

void IRAM_ATTR ReedISR()
{
	reedPulses++;
}

void EmulationTask(void* args)
{
	while (true)
	{
		//Simulate X kph wheel speed.
		float_t desiredKph = 5.0f;
		
		float_t metersPerMinute = (desiredKph * 1000) / 60;
		float_t rpm = metersPerMinute / (settings.wheelCircumference / 1000);
		float_t pulsesPerSecond = rpm / 60;
		uint delay = pdMS_TO_TICKS(1000 / pulsesPerSecond);

		reedPulses++;

		vTaskDelay(delay);
	}
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
			//Disable the interrupt while calculating the RPM to avoid incorrect values.
			detachInterrupt(REED_NEGATIVE);

			updateLatency = timeBetweenUpdates;

			//Calculate the RPM.
			//The 60000 division is to convert from milliseconds to minutes.
			realRpm = reedPulses * (60000.0 / timeBetweenUpdates);

			//Calculate the real wheel speed. The wheel circumference is needed in meters for this calculation so divide by 1000.
			//This then gives us meters per minute, to convert it to KM/H we multiply by 0.06.
			realKph = realRpm * (settings.wheelCircumference / 1000) * 0.06;

			//Reset counter and update last interval.
			reedPulses = 0;
			lastRPMUpdate = now;

			//Resume ISR.
			attachInterrupt(REED_NEGATIVE, ReedISR, FALLING);
		}

		vTaskDelay(pdMS_TO_TICKS(1));
	}
}

void RelayTask(void* args)
{
	while (true)
	{
		//If we are idle then don't relay any speed information.
		//On this chip if we ignored this the calculations would underflow and cause the delay to be uint.max.
		if (realKph <= 0)
		{
			vTaskDelay(pdMS_TO_TICKS(1));
			continue;
		}

		//Check if we should enable the speed multiplier.
		fakedKph = realKph;
		if (settings.enabled && realKph >= settings.enableMultiplierAtSpeed)
		{
			fakedKph *= settings.speedMultiplier;

			//Simulate X kph wheel speed.
			float_t metersPerMinute = (fakedKph * 1000) / 60;
			fakedRpm = metersPerMinute / (settings.wheelCircumference / 1000);
		}
		else
		{
			fakedRpm = realRpm;
		}

		float_t pulsesPerSecond = fakedRpm / 60.0f;
		uint delay = (1000.0f / pulsesPerSecond) - settings.pulseDuration;

		digitalWrite(RELAY_BASE, HIGH);
		vTaskDelay(pdMS_TO_TICKS(settings.pulseDuration));
		digitalWrite(RELAY_BASE, LOW);

		vTaskDelay(pdMS_TO_TICKS(delay));
	}
}

void LoggingTask(void* args)
{
	while (true)
	{
		std::cout
		<< "Real RPM: " << realRpm << "\n"
		<< "Real KM/H: " << realKph << "\n"
		<< "Faked RPM: " << fakedRpm << "\n"
		<< "Faked KM/H: " << fakedKph << "\n"
		<< "Update latency (ms): " << updateLatency << "\n"
		<< std::flush;

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void OnGet_Settings(AsyncWebServerRequest* request)
{
	char buf[384];
	sprintf(buf,
		SETTINGS_API_TEMPLATE,
		settings.wheelCircumference,
		settings.enabled,
		settings.speedMultiplier,
		settings.enableMultiplierAtSpeed,
		settings.pulseDuration,
		settings.timeBetweenUpdates,
		settings.revolutionsBetweenUpdates
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
			&tmpSettings.pulseDuration,
			&tmpSettings.timeBetweenUpdates,
			&tmpSettings.revolutionsBetweenUpdates
		);

		free(request->_tempObject);

		if (result != 4)
		{
        	request->send(400, "text/plain", "Invalid request.");
			return;
		}

		settings = tmpSettings;

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

void setup()
{
	Serial.begin(115200);

	pinMode(REED_POSITIVE, OUTPUT);
	pinMode(REED_NEGATIVE, INPUT_PULLDOWN);
	pinMode(RELAY_BASE, OUTPUT);
	pinMode(GPIO_NUM_3, INPUT_PULLDOWN);
	digitalWrite(REED_POSITIVE, HIGH);

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
	if (preferences.isKey("pd"))
		settings.pulseDuration = preferences.putUInt("pd", settings.pulseDuration);
	else
		preferences.getUInt("pd", settings.pulseDuration);
	if (preferences.isKey("tbu"))
		settings.timeBetweenUpdates = preferences.putUInt("tbu", settings.timeBetweenUpdates);
	else
		preferences.getUInt("tbu", settings.timeBetweenUpdates);
	if (preferences.isKey("rbu"))
		settings.revolutionsBetweenUpdates = preferences.putUInt("rbu", settings.revolutionsBetweenUpdates);
	else
		preferences.getUInt("rbu", settings.revolutionsBetweenUpdates);

	preferences.begin("reed");
	SPIFFS.begin(true);

	WiFi.mode(WIFI_AP);
	WiFi.softAP("ESP-REED", NULL, 1 /*Don't care about this value (default used)*/, 0 /*Need to set so SSID is hidden*/);
	//https://github.com/esphome/issues/issues/4893
	WiFi.setTxPower(WIFI_POWER_8_5dBm);

	server.begin();

	ElegantOTA.begin(&server);

	server.on("/", HTTP_GET, [](AsyncWebServerRequest* request){ request->send(SPIFFS, "/index.html", "text/html"); });
	server.on("/settings", HTTP_GET, OnGet_Settings);
	server.on("/settings", HTTP_POST, NULL, NULL, OnPost_Settings);
	server.on("/stats", HTTP_GET, OnGet_Stats);
	// //https://arduino.stackexchange.com/questions/89688/generalize-webserver-routing
	// server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

	//Create my own loop task as the default task has too high of a priority and prevents other secondary tasks from running.
	attachInterrupt(REED_NEGATIVE, ReedISR, FALLING);
	xTaskCreate(ReedTask, NAMEOF(ReedTask), configMINIMAL_STACK_SIZE + 2048, NULL, (UBaseType_t)(configMAX_PRIORITIES * 0.4f), NULL);
	xTaskCreate(RelayTask, NAMEOF(RelayTask), configMINIMAL_STACK_SIZE + 2048, NULL, (UBaseType_t)(configMAX_PRIORITIES * 0.5f), NULL);
	xTaskCreate(LoggingTask, NAMEOF(LoggingTask), configMINIMAL_STACK_SIZE + 4096, NULL, (UBaseType_t)(configMAX_PRIORITIES * 0.2f), NULL);
	xTaskCreate(EmulationTask, NAMEOF(EmulationTask), configMINIMAL_STACK_SIZE + 512, NULL, 5, NULL);
}

void loop()
{
	vTaskDelete(NULL);
}
