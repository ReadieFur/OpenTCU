#pragma once

#include <cstdarg>
#include <cstdio>
#ifdef DEBUG
#include <ESPAsyncWebServer.h>
#include <WebSerialLite.h>
#endif

class Logger
{
private:
    static bool _initialized;
    #ifdef DEBUG
    static AsyncWebServer _server;
    #endif

public:
    static void Begin()
    {
        if (_initialized)
            return;
        _initialized = true;

        #ifdef DEBUG
        Serial.begin(115200);
        //Give me time to connect to the serial monitor (the connection takes a moment so the first few message can be missed).
        delay(5000);

        //Ensure that a WiFi mode has been set.
        if (WiFi.getMode() == WIFI_MODE_NULL)
            WiFi.mode(WIFI_STA);

        WebSerial.begin(&_server);
        _server.begin();
        #endif

        puts("Logger initialized.");
    }

    static void Log(const char* format, ...)
    {
        char buffer[64];

        //Use variadic arguments to format the string.
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        //Print the string.
        fputs(buffer, stdout);
        #ifdef DEBUG
        WebSerial.print(buffer);
        #endif
    }
};

bool Logger::_initialized = false;
#ifdef DEBUG
AsyncWebServer Logger::_server(81);
#endif
