#include <Arduino.h>
#include <iostream>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <Preferences.h>
#include <SPIFFS.h>
#include <cstring>

#define REED_POSITIVE GPIO_NUM_0
#define REED_NEGATIVE GPIO_NUM_1
#define RELAY_BASE GPIO_NUM_2

Preferences preferences;
AsyncWebServer server(80);
struct SSettings
{
	uint wheelCircumference; //In millimeters.
	bool enabled;
	float_t speedMultiplier; //How much to raise the top speed by.
	double_t enableMultiplierAtSpeed; //In KM/H.
} settings = {
	.wheelCircumference = 2145,
	.enabled = true,
	.speedMultiplier = 1.5f,
	.enableMultiplierAtSpeed = 20.0
};

const char* apiSettingsStringTemplate = "wheelCircumference:%u,enabled:%d,speedMultiplier:%f,enableMultiplierAtSpeed:%lf";

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
	SPIFFS.begin(false);

	WiFi.mode(WIFI_AP);
	WiFi.softAP("ESP-REED", "REED", 1 /*Don't care about this value (default used)*/, 1 /*Need to set so SSID is hidden*/);
	//https://github.com/esphome/issues/issues/4893
	WiFi.setTxPower(WIFI_POWER_8_5dBm);

	ElegantOTA.begin(&server);

	server.on("/", HTTP_GET, [](AsyncWebServerRequest* request){ request->send(SPIFFS, "/index.html", "text/html"); });
	server.on("/settings", HTTP_GET, OnGet_Settings);
	server.on("/settings", HTTP_POST, NULL, NULL, OnPost_Settings);
	// //https://arduino.stackexchange.com/questions/89688/generalize-webserver-routing
	// server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

	server.begin();
}

void loop()
{
	int enabled = digitalRead(REED_NEGATIVE);
	digitalWrite(RELAY_BASE, enabled);
	#if true
	std::cout << "Reed:" << enabled << std::endl;
	#endif
	delay(10); //Prevent the CPU from running too fast (adds extra calls).
}
