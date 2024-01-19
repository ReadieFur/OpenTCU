#pragma once

#include <cstdarg>
#include <cstdio>
#ifdef DEBUG
#include <esp_log.h>
#include <ESPAsyncWebServer.h>
#include <WebSerialLite.h>
#endif

//TODO: Create a log queue that runs on a different task to avoid blocking the main task.
class Logger
{
public:
    static void Log(const char* format, ...)
    {
        char buffer[64];

        //Use variadic arguments to format the string.
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        //Print the string.
        #ifdef DEBUG
        ESP_LOGV("", buffer);
        WebSerial.print(buffer);
        #else
        fputs(buffer, stdout);
        #endif
    }
};
