#pragma once

#ifdef DEBUG
#ifdef LOG_LOCAL_LEVEL
#undef LOG_LOCAL_LEVEL
#endif
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#endif

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <driver/gpio.h>

#ifdef DEBUG
#include <SmartLeds.h>

#define _LED_BRIGHTNESS_MULTIPLIER 0.05F
#define _CONVERT_LED_VALUE(value) static_cast<uint8_t>(((value < 0.0) ? 0 : ((value > 1.0) ? 255 : (value * 255))) * _LED_BRIGHTNESS_MULTIPLIER)
#endif

class Helpers
{
private:
    #ifdef DEBUG
    static SmartLed _led;
    #endif

public:
    static void ConfigureLED()
    {
        #ifndef DEBUG
        gpio_config_t ledPinConfig = {
            .pin_bit_mask = 1ULL << LED_PIN,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        assert(gpio_config(&ledPinConfig) == ESP_OK);
        #endif
    }

    static void SetLed(bool on)
    {
        #ifdef DEBUG
        _led[0] = Rgb{_CONVERT_LED_VALUE(on ? 1 : 0), _CONVERT_LED_VALUE(on ? 1 : 0), _CONVERT_LED_VALUE(on ? 1 : 0)};
        _led.show();
        #else
        gpio_set_level(LED_PIN, on ? 1 : 0);
        #endif
    }

    static void SetLed(uint8_t r, uint8_t g, uint8_t b)
    {
        #ifdef DEBUG
        _led[0] = Rgb{_CONVERT_LED_VALUE(r), _CONVERT_LED_VALUE(g), _CONVERT_LED_VALUE(b)};
        _led.show();
        #else
        SetLed(r > 0 || g > 0 || b > 0);
        #endif
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
        
        #ifdef ENABLE_DEBUG_SERVER
        if (level == ESP_LOG_VERBOSE)
            WebSerial.printf("[%s] %s\n", tag, buffer);
        #endif

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

            SetLed(1, 0, 0);
            
            abort();
        }
    }
};

#ifdef DEBUG
SmartLed Helpers::_led(LED_WS2812, 1, LED_PIN, 0, DoubleBuffer);
#endif

#define LOG_WRAPPER(level, format, ...) Helpers::Log(level, pcTaskGetTaskName(NULL), format, ##__VA_ARGS__);

#define INFO(format, ...) LOG_WRAPPER(ESP_LOG_INFO, format, ##__VA_ARGS__);

#define ERROR(format, ...) LOG_WRAPPER(ESP_LOG_ERROR, format, ##__VA_ARGS__);

#define WARN(format, ...) LOG_WRAPPER(ESP_LOG_WARN, format, ##__VA_ARGS__);

#ifdef DEBUG
#define TRACE(format, ...) LOG_WRAPPER(ESP_LOG_VERBOSE, "[%s:%d] " format, __FILE__, __LINE__, ##__VA_ARGS__);
#else
#define TRACE(format, ...)
#endif

#ifdef DEBUG
#define ASSERT(condition) Helpers::Assert(pcTaskGetTaskName(NULL), condition, "[%s:%d] Assertion failed: %s", __FILE__, __LINE__, #condition);
#else
#define ASSERT(condition) Helpers::Assert(pcTaskGetTaskName(NULL), condition, "Assertion failed: %s", #condition);
#endif
