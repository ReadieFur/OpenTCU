#pragma once

#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <WebSerialLite.h>
#include "Logger.hpp"

#if defined(DEBUG)
//Define a trace macro that includes the line number and file name with the log.
#define TRACE(format, ...) Logger::Log("[%s:%d] " format, __FILE__, __LINE__, ##__VA_ARGS__)
#else
//Define a trace macro that does nothing.
#define TRACE(format, ...)
#endif

class Debug
{
private:
    static bool _initialized;
    static AsyncWebServer* _debugServer;

public:
    static void Setup()
    {
        if (_initialized)
            return;
        _initialized = true;

        _debugServer = new AsyncWebServer(81);

        IPAddress ipAddress;
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        if (WiFi.waitForConnectResult() != WL_CONNECTED)
        {
            printf("STA Failed!\n");

            //Create AP instead.
            WiFi.mode(WIFI_AP);
            //Create AP using mac address.
            uint8_t mac[6];
            WiFi.macAddress(mac);
            char ssid[18];
            sprintf(ssid, "ESP32-%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            WiFi.softAP(ssid);
            ipAddress = WiFi.softAPIP();
        }
        else
        {
            ipAddress = WiFi.localIP();
        }

        WebSerial.begin(_debugServer);
        _debugServer->begin();

        printf("Debug server started at %s\n", ipAddress.toString().c_str());
    }

    static void Blip(int count = 1)
    {
        for (int i = 0; i < count; i++)
        {
            digitalWrite(LED_PIN, ON);
            delay(100);
            digitalWrite(LED_PIN, OFF);
            if (i < count - 1)
                delay(100);
        }
    }
};

bool Debug::_initialized = false;
AsyncWebServer* Debug::_debugServer = nullptr;
