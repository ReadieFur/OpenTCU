#pragma once

#ifdef DEBUG
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_task_wdt.h>
#include <freertos/queue.h>
#include "CAN/SCanMessage.h"
#include "Helpers.hpp"

// #define DISABLE_WATCHDOG_TIMER
// #define VERY_VERBOSE
#define ENABLE_CAN_DUMP
#define ENABLE_POWER_CHECK
// #define ENABLE_DEBUG_SERVER

class Debug
{
private:
    static bool initialized;

    static void CanDump(void* arg)
    {
        #ifdef DISABLE_WATCHDOG_TIMER
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
            SCanDump dump;
            if (xQueueReceive(canDumpQueue, &dump, portMAX_DELAY) == pdTRUE)
            {
                printf("[CAN]%d,%d,%x,%d,%d,%d,%x,%x,%x,%x,%x,%x,%x,%x\n",
                    dump.timestamp,
                    dump.isSPI,
                    dump.message.id,
                    dump.message.isExtended,
                    dump.message.isRemote,
                    dump.message.length,
                    dump.message.data[0],
                    dump.message.data[1],
                    dump.message.data[2],
                    dump.message.data[3],
                    dump.message.data[4],
                    dump.message.data[5],
                    dump.message.data[6],
                    dump.message.data[7]
                );
            }
        }
    }

    static void PowerCheck(void* arg)
    {
        vTaskDelay(5000 / portTICK_PERIOD_MS); //Wait 5 seconds before starting.
        while (true)
        {
            #ifdef VERY_VERBOSE
            TRACE("Power check pin: %d", gpio_get_level(BIKE_POWER_CHECK_PIN));
            #endif
            if (gpio_get_level(BIKE_POWER_CHECK_PIN) == 0)
            {
                TRACE("Powering on");
                gpio_set_level(BIKE_POWER_PIN, 1);
                vTaskDelay(250 / portTICK_PERIOD_MS);
                gpio_set_level(BIKE_POWER_PIN, 0);
            }
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }
    }

    static void InitTask(void* param)
    {
        INFO("Debug setup started.");

        #ifdef ENABLE_CAN_DUMP
        xTaskCreate(CanDump, "CanDump", 4096, NULL, 1, NULL);
        #endif

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
            .intr_type = GPIO_INTR_DISABLE
        };
        assert(gpio_config(&powerPinConfig) == ESP_OK);
        assert(gpio_config(&powerCheckPinConfig) == ESP_OK);
        xTaskCreate(PowerCheck, "PowerCheck", 2048, NULL, 1, NULL);
        #endif

        #ifdef ENABLE_DEBUG_SERVER
        _debugServer = new AsyncWebServer(80);

        IPAddress ipAddress;
        WiFi.mode(WIFI_STA);
        // IPAddress ip(192, 168, 0, 158);
        // IPAddress subnet(255, 255, 254, 0);
        // IPAddress gateway(192, 168, 1, 254);
        // WiFi.config(ip, gateway, subnet);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        //https://github.com/esphome/issues/issues/4893
        WiFi.setTxPower(WIFI_POWER_8_5dBm);
        if (WiFi.waitForConnectResult() != WL_CONNECTED)
        {
            TRACE("STA Failed!");

            //Create AP instead.
            WiFi.mode(WIFI_AP);
            //Create AP using mac address.
            uint8_t mac[6];
            WiFi.macAddress(mac);
            char ssid[18];
            sprintf(ssid, "ESP32-%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            WiFi.softAP(ssid);
            ipAddress = WiFi.softAPIP();
        }
        else
        {
            ipAddress = WiFi.localIP();
        }

        WebSerial.begin(_debugServer);
        _debugServer->begin();

        TRACE("Debug server started at %s", ipAddress.toString().c_str());
        #endif

        INFO("Debug setup finished.");
        vTaskDelete(NULL);
    }

public:
    static QueueHandle_t canDumpQueue;

    struct SCanDump
    {
        uint32_t timestamp;
        bool isSPI;
        SCanMessage message;
    };

    static void Init()
    {
        if (initialized)
            return;
        initialized = true;

        esp_log_level_set("*", ESP_LOG_VERBOSE);
        xTaskCreate(InitTask, "DebugSetup", 4096, NULL, 1, NULL); //Low priority task as it is imperative that the CAN bus is setup first.
    }
};

bool Debug::initialized = false;
QueueHandle_t Debug::canDumpQueue = xQueueCreate(100, sizeof(Debug::SCanDump));
#endif
