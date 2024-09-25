#pragma once

// #if defined(DEBUG) || defined(TEST)
// #ifdef LOG_LOCAL_LEVEL
// #undef LOG_LOCAL_LEVEL
// #endif
// #define LOG_LOCAL_LEVEL DEBUG_LOG_LEVEL
// #endif

#include <esp_log.h>
#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include <iostream>

// #define ASSERT(x) ESP_ERROR_CHECK(x);

#define PRINT(format, ...) Logging::Print(format, ##__VA_ARGS__)

#define LOG(level, format, ...) Logging::Log(level, pcTaskGetTaskName(NULL), format, ##__VA_ARGS__);

#define LOG_ERROR(format, ...) LOG(ESP_LOG_ERROR, format, ##__VA_ARGS__);
#define LOG_WARN(format, ...) LOG(ESP_LOG_WARN, format, ##__VA_ARGS__);
#define LOG_INFO(format, ...) LOG(ESP_LOG_INFO, format, ##__VA_ARGS__);

#ifdef DEBUG
#define LOG_DEBUG(format, ...) LOG(ESP_LOG_DEBUG, "[%s:%d] " format, __FILE__, __LINE__, ##__VA_ARGS__);
#define LOG_TRACE(format, ...) LOG(ESP_LOG_VERBOSE, "[%s:%d] " format, __FILE__, __LINE__, ##__VA_ARGS__);
#define ASSERT(condition) Logging::Assert(pcTaskGetTaskName(NULL), condition, "[%s:%d] Assertion failed: %s", __FILE__, __LINE__, #condition);
#else
#define LOG_DEBUG(format, ...)
#define LOG_TRACE(format, ...)
#define ASSERT(condition) Logging::Assert(pcTaskGetTaskName(NULL), condition, "Assertion failed: %s", #condition);
#endif

class Logging
{
public:
    static void Print(const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        
        // ESP_LOG_LEVEL_LOCAL(level, tag, format, args);
        //Pre-format the message (the above has compiler issues).
        char buffer[128];
        vsnprintf(buffer, sizeof(buffer), format, args);
        std::cout << buffer << std::flush;

        va_end(args);
    }

    static void Log(esp_log_level_t level, char* tag, const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        
        // ESP_LOG_LEVEL_LOCAL(level, tag, format, args);
        //Pre-format the message (the above has compiler issues).
        char buffer[128];
        vsnprintf(buffer, sizeof(buffer), format, args);
        ESP_LOG_LEVEL_LOCAL(level, tag, "%s", buffer);

        va_end(args);
    }

    static void Assert(char* tag, bool condition, const char* format, ...)
    {
        if (!condition)
        {
            va_list args;
            va_start(args, format);
            
            // ESP_LOG_LEVEL_LOCAL(ESP_LOG_ERROR, tag, format, args);
            //Pre-format the message (the above has compiler issues).
            char buffer[128];
            vsnprintf(buffer, sizeof(buffer), format, args);
            ESP_LOG_LEVEL_LOCAL(ESP_LOG_ERROR, tag, "%s", buffer);

            va_end(args);
            
            abort();
        }
    }
};
