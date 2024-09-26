#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_task_wdt.h>
#include <freertos/queue.h>
#include <queue>
#include "CAN/SCanMessage.h"
#include <SPIFFS.h>
#include "CAN/BusMaster.hpp"
#include <vector>
#include <esp_check.h>
#include <esp_log.h>

class Debug
{
private:
    #ifdef ENABLE_CAN_DUMP
    static std::queue<BusMaster::SCanDump> _canDumpBuffer;
    static ulong _lastCanDump;

    static void CanDump(void* arg)
    {
        #if false
        //Disable the watchdog timer as this method seems to cause the idle task to hang.
        //This isn't ideal but won't be used for production so it'll make do for now.
        //https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/wdts.html
        esp_task_wdt_config_t config = {
            .timeout_ms = 0,
            .idle_core_mask = 0,
            .trigger_panic = false
        };
        esp_task_wdt_reconfigure(&config);
        // esp_task_wdt_deinit();
        #endif

        while (true)
        {
            BusMaster::SCanDump dump;
            if (xQueueReceive(BusMaster::canDumpQueue, &dump, pdMS_TO_TICKS(1000)) == pdTRUE)
                _canDumpBuffer.push(dump);

            if (millis() - _lastCanDump > 1000)
            {
                while (!_canDumpBuffer.empty())
                {
                    //Test this, if I get buffer overflows then use safe C-style strings like std::String or String (more overhead but safer).
                    const size_t canDumpMessageLength = 47;
                    const size_t canDumpBatchCount = 10;

                    char bulkBuffer[canDumpMessageLength * canDumpBatchCount + 1] = {0}; //Send in batches of <canDumpBatchCount>. +1 for \0 character.

                    for (int i = 0; i < canDumpBatchCount; i++)
                    {
                        if (_canDumpBuffer.empty())
                            break;

                        BusMaster::SCanDump currentDump = _canDumpBuffer.front();

                        char buffer[canDumpMessageLength + 1]; //Known data size, should never exceed this buffer (+1 for \0).
                        sprintf(buffer, "[CAN]%li,%d,%x,%d,%d,%d,%x,%x,%x,%x,%x,%x,%x,%x",
                            currentDump.timestamp,
                            currentDump.isSPI,
                            currentDump.message.id,
                            currentDump.message.isExtended,
                            currentDump.message.isRemote,
                            currentDump.message.length,
                            currentDump.message.data[0],
                            currentDump.message.data[1],
                            currentDump.message.data[2],
                            currentDump.message.data[3],
                            currentDump.message.data[4],
                            currentDump.message.data[5],
                            currentDump.message.data[6],
                            currentDump.message.data[7]);

                        _canDumpBuffer.pop();

                        strcat(bulkBuffer, buffer);
                    }

                    ESP_LOGI("can", "%s", bulkBuffer);
                }
            }
        }
    }
    #endif

    #ifdef ENABLE_POWER_CHECK
    static void PowerCheck(void* arg)
    {
        #if ENABLE_POWER_CHECK == 0
        // esp_log_level_set(pcTaskGetTaskName(NULL), DEBUG_LOG_LEVEL);

        vTaskDelay(5000 / portTICK_PERIOD_MS); //Wait 5 seconds before starting.
        while (true)
        {
            int level = gpio_get_level(BIKE_POWER_CHECK_PIN);
            DEBUG("Powered: %d", level);
            if (level == 0)
            {
        #endif
                INFO("Powering on");
                gpio_set_level(BIKE_POWER_PIN, 1);
                vTaskDelay(250 / portTICK_PERIOD_MS);
                gpio_set_level(BIKE_POWER_PIN, 0);
        #if ENABLE_POWER_CHECK == 0
            }
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }
        #endif
    }
    #endif

    static void InitTask(void* param)
    {
        // esp_log_level_set("*", DEBUG_LOG_LEVEL);
        ESP_LOGD("dbg", "Log level set to: %d", esp_log_level_get("*"));

        ESP_LOGI("dbg", "Debug setup started.");
        ESP_LOGW("dbg", "Debug setup started.");
        ESP_LOGE("dbg", "Debug setup started.");
        ESP_LOGD("dbg", "Debug setup started.");
        //ESP_LOGV("dbg", "Debug setup started.");

        #ifdef ENABLE_POWER_CHECK
        gpio_config_t powerPinConfig = {
            .pin_bit_mask = 1ULL << BIKE_POWER_PIN,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config_t powerCheckPinConfig = {
            .pin_bit_mask = 1ULL << BIKE_POWER_CHECK_PIN,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            #if ENABLE_POWER_CHECK == 0
            .intr_type = GPIO_INTR_DISABLE
            #else
            .intr_type = GPIO_INTR_NEGEDGE
            #endif
        };
        assert(gpio_config(&powerPinConfig) == ESP_OK);
        assert(gpio_config(&powerCheckPinConfig) == ESP_OK);
        gpio_install_isr_service(0);
        #if ENABLE_POWER_CHECK == 0
        xTaskCreate(PowerCheck, "PowerCheck", 2048, NULL, 1, NULL);
        #else
        assert(gpio_isr_handler_add(BIKE_POWER_CHECK_PIN, PowerCheck, NULL) == ESP_OK);
        if (gpio_get_level(BIKE_POWER_CHECK_PIN) == 0)
            PowerCheck(NULL);
        #endif
        #endif

        #ifdef ENABLE_CAN_DUMP
        xTaskCreate(CanDump, "CanDump", 4096, NULL, 1, NULL);
        #endif

        //ESP_LOGV("dbg", "Debug setup finished.");
        vTaskDelete(NULL);
    }

public:
    static void Init()
    {
        xTaskCreate(InitTask, "DebugSetup", 4096, NULL, 2, NULL); //Low priority task as it is imperative that the CAN bus gets the most CPU time.
    }
};

#ifdef ENABLE_CAN_DUMP
std::queue<BusMaster::SCanDump> Debug::_canDumpBuffer;
ulong Debug::_lastCanDump = 0;
#endif
