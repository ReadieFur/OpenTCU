#pragma once

#if defined(_TEST)
#define LOG_LEVEL ESP_LOG_VERBOSE
#elif defined(_DEBUG)
#define LOG_LEVEL ESP_LOG_DEBUG
#endif

#if defined(_DEBUG) || defined(_TEST)
#ifdef LOG_LOCAL_LEVEL
#undef LOG_LOCAL_LEVEL
#endif
#define LOG_LOCAL_LEVEL LOG_LEVEL
#endif

#include <esp_log.h>
#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include <freertos/portmacro.h>
#include <driver/gpio.h>

#ifdef _DEBUG
#include "Debug.hpp"
#include <SmartLeds.h>
#include <WebSerialLite.h>

#define _LED_BRIGHTNESS_MULTIPLIER 0.05F
#define _CONVERT_LED_VALUE(value) static_cast<uint8_t>(((value < 0.0) ? 0 : ((value > 1.0) ? 255 : (value * 255))) * _LED_BRIGHTNESS_MULTIPLIER)
#endif

#define LOG_WRAPPER(level, format, ...) Helpers::Log(level, pcTaskGetTaskName(NULL), format, ##__VA_ARGS__);

#define FATAL(format, ...) LOG_WRAPPER(ESP_LOG_ERROR, format, ##__VA_ARGS__); abort();
#define ERROR(format, ...) LOG_WRAPPER(ESP_LOG_ERROR, format, ##__VA_ARGS__);
#define WARN(format, ...) LOG_WRAPPER(ESP_LOG_WARN, format, ##__VA_ARGS__);
#define INFO(format, ...) LOG_WRAPPER(ESP_LOG_INFO, format, ##__VA_ARGS__);

#ifdef _DEBUG
#define DEBUG(format, ...) LOG_WRAPPER(ESP_LOG_DEBUG, "[%s:%d] " format, __FILE__, __LINE__, ##__VA_ARGS__);
#define TRACE(format, ...) LOG_WRAPPER(ESP_LOG_VERBOSE, "[%s:%d] " format, __FILE__, __LINE__, ##__VA_ARGS__);
#else
#define DEBUG(format, ...)
#define TRACE(format, ...)
#endif

#ifdef _DEBUG
#define ASSERT(condition) Helpers::Assert(pcTaskGetTaskName(NULL), condition, "[%s:%d] Assertion failed: %s", __FILE__, __LINE__, #condition);
#else
#define ASSERT(condition) Helpers::Assert(pcTaskGetTaskName(NULL), condition, "Assertion failed: %s", #condition);
#endif

class Helpers
{
private:
    #ifdef _DEBUG
    static SmartLed _led;
    #endif

public:
    static void ConfigureLED()
    {
        gpio_config_t ledPinConfig = {
            .pin_bit_mask = 1ULL << LED_PIN,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        assert(gpio_config(&ledPinConfig) == ESP_OK);
    }

    static void SetLed(bool on)
    {
        #ifdef _DEBUG
        _led[0] = Rgb{_CONVERT_LED_VALUE(on ? 1 : 0), _CONVERT_LED_VALUE(on ? 1 : 0), _CONVERT_LED_VALUE(on ? 1 : 0)};
        _led.show();
        #else
        gpio_set_level(LED_PIN, on ? 1 : 0);
        #endif
    }

    static void SetLed(uint8_t r, uint8_t g, uint8_t b)
    {
        #ifdef _DEBUG
        _led[0] = Rgb{_CONVERT_LED_VALUE(r), _CONVERT_LED_VALUE(g), _CONVERT_LED_VALUE(b)};
        _led.show();
        #else
        SetLed(r > 0 || g > 0 || b > 0);
        #endif
    }

    static void Log(esp_log_level_t level, char* tag, const char* format, ...)
    {
        #ifdef _DEBUG
        Rgb previousColor = _led[0];
        Rgb statusColor;
        switch (level)
        {
        case ESP_LOG_ERROR:
            statusColor = Rgb{_CONVERT_LED_VALUE(1), 0, 0};
            break;
        case ESP_LOG_WARN:
            statusColor = Rgb{_CONVERT_LED_VALUE(1), _CONVERT_LED_VALUE(1), 0};
            break;
        case ESP_LOG_INFO:
            statusColor = Rgb{0, _CONVERT_LED_VALUE(0.75), _CONVERT_LED_VALUE(1)};
            break;
        case ESP_LOG_DEBUG:
            statusColor = Rgb{_CONVERT_LED_VALUE(0.75), 0, _CONVERT_LED_VALUE(1)};
            break;
        case ESP_LOG_VERBOSE:
            statusColor = Rgb{0, _CONVERT_LED_VALUE(1), _CONVERT_LED_VALUE(0.5)};
            break;
        default:
            statusColor = previousColor;
            break;
        }
        SetLed(statusColor.r, statusColor.g, statusColor.b);
        #endif

        va_list args;
        va_start(args, format);
        
        // ESP_LOG_LEVEL_LOCAL(level, tag, format, args);
        //Pre-format the message (the above has compiler issues).
        char buffer[128];
        vsnprintf(buffer, sizeof(buffer), format, args);
        ESP_LOG_LEVEL_LOCAL(level, tag, "%s", buffer);
        
        #if defined(ENABLE_WIFI_SERVER) && 0
        if (level == ESP_LOG_DEBUG)
            WebSerial.printf("[%s] %s\n", tag, buffer);
        #endif

        va_end(args);

        #if _DEBUG
        SetLed(previousColor.r, previousColor.g, previousColor.b);
        #endif
    }

    static void Assert(char* tag, bool condition, const char* format, ...)
    {
        if (!condition)
        {
            SetLed(1, 0, 0);

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

#ifdef _DEBUG
SmartLed Helpers::_led(LED_WS2812, 1, LED_PIN, 0, DoubleBuffer);
#endif
