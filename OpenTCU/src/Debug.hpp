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
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        if (WiFi.waitForConnectResult() != WL_CONNECTED)
        {
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            printf("WiFi Failed!\n");
            vTaskDelete(NULL);
        }
        WebSerial.begin(_debugServer);
        _debugServer->begin();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        printf("Debug server started at %s\n", WiFi.localIP().toString().c_str());
    }
};

bool Debug::_initialized = false;
AsyncWebServer* Debug::_debugServer = nullptr;
