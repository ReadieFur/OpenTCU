#pragma once

//TODO: Clean up this file.

#ifdef DEBUG
#ifdef LOG_LOCAL_LEVEL
#undef LOG_LOCAL_LEVEL
#endif
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#endif

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

#define NEWLINE() printf("\n");

#ifdef DEBUG
#include <SmartLeds.h>
SmartLed _led(LED_WS2812, 1, LED_PIN, 0, DoubleBuffer);
#define _LED_BRIGHTNESS_MULTIPLIER 0.05F
#define _LED_UINT8(value) static_cast<uint8_t>(((value < 0.0) ? 0 : ((value > 1.0) ? 255 : (value * 255))) * _LED_BRIGHTNESS_MULTIPLIER)
#define LED(r, g, b) _led[0] = Rgb{_LED_UINT8(r), _LED_UINT8(g), _LED_UINT8(b)}; _led.show();
#endif

//Log method should do a simple log to the serial.
// ESP_LOGI(__FILE__, format, ##__VA_ARGS__);
// #define LOG(format, ...) printf(format, ##__VA_ARGS__); NEWLINE();
#define INFO(format, ...) ESP_LOGI(pcTaskGetTaskName(NULL), format, ##__VA_ARGS__);

#define ERROR(format, ...) ESP_LOGE(pcTaskGetTaskName(NULL), format, ##__VA_ARGS__);

#define WARN(format, ...) ESP_LOGW(pcTaskGetTaskName(NULL), format, ##__VA_ARGS__);

//Trace method should do a log to the serial and the web serial with the line number and file name when in debug mode.
#ifdef DEBUG
// printf("[%s:%d] " format, __FILE__, __LINE__, ##__VA_ARGS__);
// ESP_LOGV(__FILE__ ":" __LINE__, format, ##__VA_ARGS__); 
#define TRACE(format, ...) \
    ESP_LOGV(pcTaskGetTaskName(NULL), format, ##__VA_ARGS__);
    // WebSerial.printf("[%s:%d] " format, __FILE__, __LINE__, ##__VA_ARGS__);
    // WebSerial.println();
#else
#define TRACE(format, ...)
#endif

#ifdef DEBUG
#define ASSERT(condition)  \
    if (!(condition)) \
    { \
        ERROR("Assertion failed: %s", #condition); \
        LED(1, 0, 0); \
        abort(); \
    }
#else
#define ASSERT(condition) \
    if (!(condition)) \
    { \
        ERROR("Assertion failed: %s", #condition); \
        abort(); \
    }
#endif

#define pdUS_TO_TICKS(xTimeInNs) ((TickType_t)(((TickType_t)(xTimeInNs) * (TickType_t)configTICK_RATE_HZ) / (TickType_t)1000000U))
