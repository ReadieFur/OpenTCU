#ifdef DEBUG
#define _CAN_TEST
#endif

#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include "pch.h"
#include "Config/Device.h"
#include "CAN/BusMaster.hpp"
#include "Config/JsonFlash.hpp"
#include <esp_sleep.h>
#include <freertos/task.h>
#include <esp_log.h>
#ifdef _CAN_TEST
#include "CAN/Test.hpp"
#endif

#define ABORT_ON_FAIL(a, format) do {                                                   \
        if (unlikely(!(a))) {                                                           \
            ESP_LOGE("main", "%s(%d): " format ": %i", __FUNCTION__, __LINE__, a);      \
            abort();                                                                    \
        }                                                                               \
    } while(0)

using namespace ReadieFur::OpenTCU;

Config::JsonFlash* _config;
CAN::BusMaster* _busMaster;

void setup()
{
    #ifdef _CAN_TEST
    vTaskDelay(pdMS_TO_TICKS(2500)); //Delay to allow for the debugger to attach.
    xTaskCreate([](void*)
    {
        while (true)
        {
            ESP_LOGI("AliveTask", "Alive");
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }, "AliveTask", CONFIG_FREERTOS_IDLE_TASK_STACKSIZE + 1024, nullptr, configMAX_PRIORITIES * 0.5, nullptr);
    #endif

    _config = Config::JsonFlash::Open("config.json");
    ABORT_ON_FAIL(_config != nullptr, "Failed to load config.");

    #ifndef _CAN_TEST
    _busMaster = new CAN::BusMaster(_config);
    #else
    _busMaster = new CAN::Test(_config);
    #endif
    ABORT_ON_FAIL(_busMaster->InstallService(), "Failed to install " nameof(CAN::BusMaster) " service.");
    ABORT_ON_FAIL(_busMaster->StartService(), "Failed to start " nameof(CAN::BusMaster) " service.");
}

void loop()
{
    //I don't need this loop method as this program is task based.
    vTaskDelete(NULL);
}

#ifndef ARDUINO
extern "C" void app_main()
{
    TaskHandle_t mainTaskHandle = xTaskGetHandle(pcTaskGetName(NULL));
    setup();
    while (eTaskGetState(mainTaskHandle) != eTaskState::eDeleted)
    {
        loop();
        portYIELD();
    }
}
#endif
