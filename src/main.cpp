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

Preferences preferences;
AsyncWebServer server(80);
const char* apiSettingsStringTemplate = "wheelCircumference:%u,enabled:%d,speedMultiplier:%f,enableMultiplierAtSpeed:%lf";
struct SSettings
{
	uint wheelCircumference; //In millimeters.
	bool enabled;
	float_t speedMultiplier; //How much to raise the top speed by.
	double_t enableMultiplierAtSpeed; //In mm/s.
} settings = {
	.wheelCircumference = 2145,
	.enabled = true,
	.speedMultiplier = 1.5f,
	.enableMultiplierAtSpeed = 20.0
};

//https://electronics.stackexchange.com/questions/89/how-do-i-measure-the-rpm-of-a-wheel
volatile uint reedPulses = 0;
ulong lastRPMUpdate = 0;
ulong realRpm = 0;

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
		if (reedPulses >= 20 || timeBetweenUpdates >= 3000)
		{
			//Disable the interrupt while calculating the RPM to avoid incorrect values.
			detachInterrupt(REED_NEGATIVE);

			//Calculate the RPM.
			//The 60000 division is to convert from milliseconds to minutes.
			realRpm = reedPulses * (60000.0 / timeBetweenUpdates);

			reedPulses = 0;
			lastRPMUpdate = now;

			//Resume ISR.
			attachInterrupt(REED_NEGATIVE, ReedISR, FALLING);

			//Logging.
			std::cout << "RPM: " << realRpm << "\n";

			//Calculate the real wheel speed. The wheel circumference is needed in meters for this calculation so divide by 1000.
			//This then gives us meters per minute, to convert it to KM/H we multiply by 0.06.
			std::cout << "KM/H: " << realRpm * (settings.wheelCircumference / 1000) * 0.06 << std::endl;
		}

		vTaskDelay(pdMS_TO_TICKS(1));
	}
}

void OnGet_Settings(AsyncWebServerRequest* request)
{
	char buf[128];
	sprintf(buf,
		apiSettingsStringTemplate,
		settings.wheelCircumference, settings.enabled, settings.speedMultiplier, settings.enableMultiplierAtSpeed);
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

		uint wheelCircumference;
		bool enabled;
		float_t speedMultiplier;
		double_t enableMultiplierAtSpeed;

		//Extract values using sscanf.
		int result = sscanf(static_cast<char*>(request->_tempObject),
			apiSettingsStringTemplate,
			&wheelCircumference, &enabled, &speedMultiplier, &enableMultiplierAtSpeed);

		free(request->_tempObject);

		if (result != 4)
		{
        	request->send(400, "text/plain", "Invalid request.");
			return;
		}

		settings.wheelCircumference = wheelCircumference;
		settings.enabled = enabled;
		settings.speedMultiplier = speedMultiplier;
		settings.enableMultiplierAtSpeed = enableMultiplierAtSpeed;
        
        request->send(200, "text/plain", "Settings received and parsed.");
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
	// if (preferences.isKey("rui"))
	// 	settings.rpmUpdateInterval = preferences.putUInt("rui", settings.rpmUpdateInterval);
	// else
	// 	preferences.getUInt("rui", settings.rpmUpdateInterval);

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
	// //https://arduino.stackexchange.com/questions/89688/generalize-webserver-routing
	// server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

	//Create my own loop task as the default task has too high of a priority and prevents other secondary tasks from running.
	attachInterrupt(REED_NEGATIVE, ReedISR, FALLING);
	xTaskCreate(ReedTask, "ReedTask", configMINIMAL_STACK_SIZE + 4096, NULL, (UBaseType_t)(configMAX_PRIORITIES * 0.5f), NULL);
	xTaskCreate(EmulationTask, "EmulationTask", configMINIMAL_STACK_SIZE + 512, NULL, 5, NULL);
}

void loop()
{
	vTaskDelete(NULL);
}
