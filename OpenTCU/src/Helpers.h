#pragma once

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

#ifdef DEBUG
#include <WebSerialLite.h>
#endif

#define ON 0x00
#define OFF 0x01

//Log method should do a simple log to the serial.
// ESP_LOGI(__FILE__, format, ##__VA_ARGS__);
#define LOG(format, ...) \
    printf(format, ##__VA_ARGS__); \
    NEWLINE();

//Trace method should do a log to the serial and the web serial with the line number and file name when in debug mode.
#ifdef DEBUG
// printf("[%s:%d] " format, __FILE__, __LINE__, ##__VA_ARGS__);
// ESP_LOGV(__FILE__ ":" __LINE__, format, ##__VA_ARGS__); 
#define TRACE(format, ...) \
    ESP_LOGV(pcTaskGetTaskName(NULL), format, ##__VA_ARGS__); \
    WebSerial.printf("[%s:%d] " format, __FILE__, __LINE__, ##__VA_ARGS__); \
    WebSerial.println();
#else
#define TRACE(format, ...)
#endif

#define NEWLINE() printf("\n");

#define pdUS_TO_TICKS(xTimeInNs) ((TickType_t)(((TickType_t)(xTimeInNs) * (TickType_t)configTICK_RATE_HZ) / (TickType_t)1000000U))
