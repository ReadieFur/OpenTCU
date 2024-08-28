#pragma once

#ifdef DEBUG
#include <freertos/FreeRTOS.h>
#include "Logging.h"
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_task_wdt.h>
#include <freertos/queue.h>
#include <queue>
#include "CAN/SCanMessage.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSerialLite.h>
#include <SPIFFS.h>
#include "CAN/BusMaster.hpp"
#include <vector>
#include <lwip/sockets.h>
#include <time.h>
#include <string>
#include <cstring>

class Debug
{
private:
    static AsyncWebServer* _webserver;

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
                        sprintf(buffer, "[CAN]%d %d,%d,%x,%d,%d,%d,%x,%x,%x,%x,%x,%x,%x,%x\n",
                            currentDump.timestamp,
                            currentDump.timestamp2,
                            currentDump.isSpi,
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

                    PRINT(bulkBuffer);
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

    static void OnStaConnect(arduino_event_t *event)
    {
        WiFi.removeEvent(OnStaConnect, ARDUINO_EVENT_WIFI_STA_CONNECTED);

        WiFi.waitForConnectResult();

        LOG_INFO("STA IP Address: %s", WiFi.localIP().toString());
            
        #ifdef NTP_SERVER
        //Time setup is done here to get the actual time, if this stage is skipped the internal ESP tick count is used for the logs.
        //Setup RTC for logging (mainly to sync external data to data recorded from this device, e.g. pairing video with recorded data).
        LOG_TRACE("Syncing time with NTP server...");
        for (int i = 0; i < 10; i++)
        {
            struct tm timeInfo;
            if (getLocalTime(&timeInfo))
            {
                long timestamp;
                time(&timestamp);
                LOG_DEBUG("Successfully synced with timeserver. %i/%i/%i %i:%i:%i %li %li",
                    timeInfo.tm_year,
                    timeInfo.tm_mon,
                    timeInfo.tm_yday,
                    timeInfo.tm_hour,
                    timeInfo.tm_min,
                    timeInfo.tm_sec,
                    timestamp,
                    millis()
                );
                break;
            }
            else if (i == 9)
            {
                LOG_ERROR("Failed to sync with timeserver.");
                break;
            }

            //https://randomnerdtutorials.com/esp32-date-time-ntp-client-server-arduino/
            configTime(0, 3600, NTP_SERVER);
            delay(500);
        }
        #endif
    }

    static void InitTask(void* param)
    {
        // esp_log_level_set("*", DEBUG_LOG_LEVEL);
        LOG_INFO("Log level set to: %d", esp_log_level_get("*"));

        LOG_INFO("Debug setup started.");
        LOG_WARN("Debug setup started.");
        LOG_ERROR("Debug setup started.");
        LOG_DEBUG("Debug setup started.");
        LOG_TRACE("Debug setup started.");

        #if defined(NTP_SERVER) && !defined(STA_WIFI_SSID)
        #error "STA Network (STA_WIFI_SSID) not specified."
        #endif

        #ifdef STA_WIFI_SSID
        switch (WiFi.getMode())
        {
        case WIFI_AP:
            WiFi.mode(WIFI_AP_STA);
            break;
        default:
            WiFi.mode(WIFI_STA);
            break;
        }
        #endif

        #if defined(STA_WIFI_SSID) && defined(STA_WIFI_PASSWORD)
        WiFi.begin(STA_WIFI_SSID, STA_WIFI_PASSWORD);
        #elif defined(STA_WIFI_SSID)
        WiFi.begin(STA_WIFI_SSID);
        #endif

        #ifdef STA_WIFI_SSID
        //https://github.com/esphome/issues/issues/4893
        WiFi.setTxPower(WIFI_POWER_8_5dBm);
        for (size_t i = 0; i < 10; i++)
        {
            if (WiFi.waitForConnectResult() == WL_CONNECTED)
                break;

            if (i == 9)
            {
                LOG_ERROR("Failed to connect to STA network (will retry).");
                WiFi.onEvent(OnStaConnect, ARDUINO_EVENT_WIFI_STA_CONNECTED);
            }

            vTaskDelay(pdMS_TO_TICKS(500));
        }
        if (WiFi.isConnected())
            OnStaConnect(NULL);
        #endif

        _webserver = new AsyncWebServer(81);
        WebSerial.begin(_webserver);
        _webserver->begin();


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
        xTaskCreate(CanDump, "CanDump", 4096, NULL, 10, NULL);
        #endif

        LOG_TRACE("Debug setup finished.");
        vTaskDelete(NULL);
    }

public:
    static void Init()
    {
        esp_log_level_set("*", DEBUG_LOG_LEVEL);
        xTaskCreate(InitTask, "DebugSetup", 4096, NULL, 2, NULL); //Low priority task as it is imperative that the CAN bus gets the most CPU time.
    }
};

AsyncWebServer* Debug::_webserver = nullptr;

#ifdef ENABLE_CAN_DUMP
std::queue<BusMaster::SCanDump> Debug::_canDumpBuffer;
ulong Debug::_lastCanDump = 0;
#endif
#endif
