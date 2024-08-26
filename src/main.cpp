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

#define REED_POSITIVE GPIO_NUM_0
#define REED_NEGATIVE GPIO_NUM_1
#define RELAY_BASE GPIO_NUM_2

void setup()
{
	Serial.begin(115200);

	pinMode(REED_POSITIVE, OUTPUT);
	pinMode(REED_NEGATIVE, INPUT_PULLDOWN);
	pinMode(RELAY_BASE, OUTPUT);
	pinMode(GPIO_NUM_3, INPUT_PULLDOWN);
	digitalWrite(REED_POSITIVE, HIGH);
}

void loop()
{
	vTaskDelete(NULL);
}
