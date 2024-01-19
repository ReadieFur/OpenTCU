#pragma once

#include <cstdarg>
#include <cstdio>
#ifdef DEBUG
#include <ESPAsyncWebServer.h>
#include <WebSerialLite.h>
#endif

#if defined(DEBUG) && 1
//Define a trace macro that includes the line number and file name with the log.
#define TRACE(format, ...) Logger::Log("[%s:%d] " format, __FILE__, __LINE__, ##__VA_ARGS__)
#else
//Define a trace macro that does nothing.
#define TRACE(format, ...)
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
        fputs(buffer, stdout);
        #ifdef DEBUG
        WebSerial.print(buffer);
        #endif
    }
};
