// #define TEST_CHIP

//https://community.platformio.org/t/esp-log-not-working-on-wemos-s2-mini-but-fine-on-s3-devkit/34717/4
#include <Arduino.h>
#define MAX_LOG_LEVEL ESP_LOG_DEBUG
#ifdef LOG_LOCAL_LEVEL
#undef LOG_LOCAL_LEVEL
#endif
#define LOG_LOCAL_LEVEL MAX_LOG_LEVEL //This overrides CONFIG_LOG_MAXIMUM_LEVEL setting in menuconfig and must be defined before including esp_log.h
#ifdef CONFIG_LOG_MAXIMUM_LEVEL
#undef CONFIG_LOG_MAXIMUM_LEVEL
#endif
#define CONFIG_LOG_MAXIMUM_LEVEL MAX_LOG_LEVEL
#include <esp_log.h>

#ifndef TEST_CHIP
#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include "Debug.hpp"
#include "CAN/BusMaster.hpp"
#include <freertos/task.h>
#endif

void SetLogLevel()
{
    //These seem to do nothing :/
    esp_log_level_set("test", ESP_LOG_DEBUG);
    esp_log_level_set("spi", ESP_LOG_VERBOSE);
    esp_log_level_set("twai", ESP_LOG_NONE);
    esp_log_level_set("bus", ESP_LOG_ERROR);
    esp_log_level_set("dbg", ESP_LOG_DEBUG);
    esp_log_level_set("can", ESP_LOG_INFO);
}

void setup()
{
    SetLogLevel();

    #ifndef TEST_CHIP
    xTaskCreate([](void*)
    {
        while(true)
        {
            // SetLogLevel();
            ESP_LOGD("test", "SERIAL_ALIVE_CHECK");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }, "AliveCheck", configIDLE_TASK_STACK_SIZE + 1024, nullptr, (configMAX_PRIORITIES * 0.35), nullptr);
    //It is critical that this is the first service to be started.
    BusMaster::Begin();
    Debug::Init();
    #else
    #endif
}

#ifdef ARDUINO
void loop()
{
    #ifndef TEST_CHIP
    vTaskDelete(NULL);
    #else
    esp_log_level_set("*", LOG_LOCAL_LEVEL);
    ESP_LOGW("test", "Hello, World!");
    vTaskDelay(pdMS_TO_TICKS(1000));
    #endif
}
#else
extern "C" void app_main()
{
    setup();
    //app_main IS allowed to return as per the ESP32 documentation (other FreeRTOS tasks will continue to run).
}
#endif
