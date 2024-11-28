#ifdef DEBUG
// #define _CAN_TEST
#define ENABLE_CAN_DUMP
#endif

#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include "pch.h"
#include "Service/ServiceManager.hpp"
#include "Config/Device.h"
#include "CAN/BusMaster.hpp"
#include "CAN/Logger.hpp"
#include <esp_sleep.h>
#include <freertos/task.h>
#include "Logging.hpp"
#ifdef _CAN_TEST
#include "CAN/Test.hpp"
#endif
#ifdef DEBUG
#include "Diagnostic/DiagnosticsService.hpp"
#endif

#define CHECK_SERVICE_RESULT(func) do {                                     \
        ReadieFur::Service::EServiceResult result = func;                   \
        if (result == ReadieFur::Service::Ok) break;                        \
        LOGE(pcTaskGetName(NULL), "Failed with result: %i", result);        \
        abort();                                                            \
    } while (0)

using namespace ReadieFur::OpenTCU;

void setup()
{
    #ifdef DEBUG
    //Set base log level.
    esp_log_level_set("*", ESP_LOG_DEBUG);
    //Set custom log levels.
    esp_log_level_set(nameof(CAN::BusMaster), ESP_LOG_ERROR);
    esp_log_level_set(nameof(CAN::TwaiCan), ESP_LOG_ERROR);
    // esp_log_level_set(nameof(CAN::McpCan), ESP_LOG_ERROR);
    esp_log_level_set(nameof(CAN::Logger), ESP_LOG_INFO);
    #else
    esp_log_level_set("*", ESP_LOG_INFO);
    #endif

    #ifdef _CAN_TEST
    xTaskCreate([](void*)
    {
        while (true)
        {
            LOGI("AliveTask", "Alive");
            vTaskDelay(pdMS_TO_TICKS(5000));
        }
    }, "AliveTask", IDLE_TASK_STACK_SIZE + 1024, nullptr, configMAX_PRIORITIES * 0.5, nullptr);
    #endif

    #ifndef _CAN_TEST
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<CAN::BusMaster>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<CAN::BusMaster>());
    #else
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<CAN::Test>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<CAN::Test>());
    #endif

    #ifdef DEBUG
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<ReadieFur::Diagnostic::DiagnosticsService>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<ReadieFur::Diagnostic::DiagnosticsService>());
    #endif

    #ifdef ENABLE_CAN_DUMP
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallService<CAN::Logger>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::StartService<CAN::Logger>());
    #endif
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
