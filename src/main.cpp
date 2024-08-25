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
} volatile settings = {
	.wheelCircumference = 2145,
	.enabled = true,
	.speedMultiplier = 1.5f,
	.enableMultiplierAtSpeed = 20.0
};

volatile unsigned long lastTime = 0;
volatile unsigned long currentTime = 0;
volatile float timePerCycle = 0.0; //Time taken for one full cycle (on-time + off-time).
volatile float speed = 0.0; //Speed in mm/s.
volatile bool pulseDetected = false;
unsigned long lastRelayTime = 0;
bool relayState = false;

void IRAM_ATTR ReedISR()
{
	currentTime = millis();
    timePerCycle = currentTime - lastTime;
    if (timePerCycle > 0)
        speed = settings.wheelCircumference / (timePerCycle / 1000.0); //Convert time to seconds and calculate speed.
    lastTime = currentTime;
    pulseDetected = true;
}

void RelayTask(void* args)
{
	while (true)
	{
		if (pulseDetected)
		{
			pulseDetected = false;

			float adjustedTimePerCycle;
			//Check if the speed is above the threshold.
            if (speed > settings.enableMultiplierAtSpeed)
			{
                //Apply multiplier.
                adjustedTimePerCycle = timePerCycle * settings.speedMultiplier;
            }
			else
			{
                //No multiplier, use the original time per cycle.
                adjustedTimePerCycle = timePerCycle;
            }

			//Use millis() for non-blocking delay logic.
			unsigned long currentMillis = millis();
			if (currentMillis - lastRelayTime >= adjustedTimePerCycle / 2)
			{
				relayState = !relayState;
				digitalWrite(RELAY_BASE, relayState ? HIGH : LOW);
				lastRelayTime = currentMillis;
			}
		}

		//Ensure task yields and does not hog CPU.
		vTaskDelay(pdMS_TO_TICKS(1));
	}
}

void LogTask(void* args)
{
	while (true)
	{
		std::cout << "Real wheel Speed: " << speed * 0.0036 << "km/h" << std::endl;
		std::cout << "Faked wheel Speed: " << (speed / settings.speedMultiplier) * 0.0036 << "km/h" << std::endl;
		// std::cout << "Output Frequency: " << 1000.0 / (timePerCycle * settings.speedMultiplier) << std::endl;

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

// void ReedTask(void* args)
// {
// 	ulong lastLogTime = 0;
// 	ulong onTime = 0;
// 	long offTime = 0;

// 	while (true)
// 	{
// 		int enabled = digitalRead(REED_NEGATIVE);
// 		digitalWrite(RELAY_BASE, enabled);
		
// 		#if true
// 		if (millis() - lastLogTime < 10)
// 		{
// 			std::cout << "Reed:" << enabled << std::endl;
// 			lastLogTime = millis();
// 		}
// 		#endif
		
// 		vTaskDelay(pdMS_TO_TICKS(10));
// 	}
// }

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
	// xTaskCreate(ReedTask, "ReedTask", configMINIMAL_STACK_SIZE + 1024, NULL, (UBaseType_t)(configMAX_PRIORITIES * 0.5f), NULL);
	attachInterrupt(REED_NEGATIVE, ReedISR, CHANGE);
	// xTaskCreate(RelayTask, "RelayTask", configMINIMAL_STACK_SIZE + 1024, NULL, (UBaseType_t)(configMAX_PRIORITIES * 0.5f), NULL);
	xTaskCreate(LogTask, "LogTask", configMINIMAL_STACK_SIZE + 4096, NULL, 2, NULL); //Large stack size needed for std::cout.
}

void loop()
{
	vTaskDelete(NULL);
}
