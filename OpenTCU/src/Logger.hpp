#pragma once

#include <cstdarg>
#include <cstdio>

#if defined(DEBUG) && 1
//Define a trace macro that includes the line number and file name with the log.
#define TRACE(format, ...) Logger::Log("[%s:%d] " format, __FILE__, __LINE__, ##__VA_ARGS__)
#else
//Define a trace macro that does nothing.
#define TRACE(format, ...)
#endif

class Logger
{
private:
    static bool _initialized;

public:
    static void Begin()
    {
        if (_initialized)
            return;
        _initialized = true;
        Log("Logger initialized.");
    }

    static void Log(const char* format, ...)
    {
        if (!_initialized)
            Begin();

        char buffer[64];

        //Use variadic arguments to format the string.
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        //Print the string.
        fputs(buffer, stdout);
    }
};

bool Logger::_initialized = false;
