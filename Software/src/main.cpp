/**
 * Copyright (c) 2026, ReadieFur. All rights reserved.
 * Licensed under the GPL-3.0 License.
 * Project: OpenTCU
 * Author: ReadieFur
 * Repository: https://github.com/ReadieFur/OpenTCU
 */

#include "ProgramConfig.h"
#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include "Service/ServiceManager.hpp"
#include "CAN/BusMaster.hpp"
#include "CAN/BusLogger.hpp"
#include <esp_sleep.h>
#include <freertos/task.h>
#include "Logging.hpp"
#ifdef DEBUG
#include "Diagnostic/DiagnosticsService.hpp"
#endif
#include <esp_pm.h>
#include "Networking/BleApi.hpp"
#include "Networking/WiFiApi.hpp"
// #include "Networking/TCU.hpp"
#include <string>
#include <esp_mac.h>
#include <cstring>
#include "Data/Flash.hpp"
#include "Data/Persistent.hpp"
#include <Event/Observable.hpp>
#include "led_strip.h"
#include "NimBLEDevice.h"

#define CHECK_SERVICE_RESULT(func) do {                                                 \
        ReadieFur::Service::EServiceResult result = func;                               \
        if (result == ReadieFur::Service::Ok) break;                                    \
        LOGE(pcTaskGetName(NULL), "[%d] Failed with result: %s", __LINE__, ReadieFur::Service::ServiceManager::ServiceResultToString(result));     \
        abort();                                                                        \
    } while (0)

#define CHECK_ESP_RESULT(func) do {                                                     \
        esp_err_t result = func;                                                        \
        if (result == ESP_OK) break;                                                    \
        LOGE(pcTaskGetName(NULL), "[%d] Failed with result: %s", __LINE__, esp_err_to_name(result));   \
        abort();                                                                        \
    } while (0)

using namespace ReadieFur::OpenTCU;

led_strip_handle_t rgbLed;

void SetCPUFrequency()
{
    // Set CPU frequency to the highest available as this real-time system needs to be as fast as possible.
    esp_pm_config_t cpuConfig = {
        .max_freq_mhz = ESP_PM_CPU_FREQ_MAX,
        .min_freq_mhz = ESP_PM_CPU_FREQ_MAX,
        .light_sleep_enable = false
    };
    CHECK_ESP_RESULT(esp_pm_configure(&cpuConfig));
    // Setting the CPU frequency is not strictly required as when TWAI is enabled, the frequency is locked at ESP_PM_APB_FREQ_MAX as per the ESP-IDF documentation.
}

void SetLogLevel()
{
    #ifdef DEBUG
    // Set base log level.
    esp_log_level_set("*", ESP_LOG_DEBUG);

    // Set custom log levels.
    esp_log_level_set(nameof(CAN::BusMaster), ESP_LOG_DEBUG);
    esp_log_level_set(nameof(CAN::TwaiCan), ESP_LOG_ERROR);
    // esp_log_level_set(nameof(CAN::McpCan), ESP_LOG_ERROR);
    esp_log_level_set(nameof(CAN::Logger), ESP_LOG_INFO);
    esp_log_level_set(nameof(OTA::API), ESP_LOG_DEBUG);
    // esp_log_level_set(nameof(Networking::TCU), ESP_LOG_DEBUG);
    #else
    esp_log_level_set("*", ESP_LOG_INFO);
    #endif
}

void ConfigureLED()
{
    // Configure static led.
    gpio_config_t staticLedIOConfig = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    staticLedIOConfig.pin_bit_mask = 1ULL << STATIC_LED_PIN;
    gpio_config(&staticLedIOConfig);
    gpio_set_level(STATIC_LED_PIN, 1); // Turn on the built-in LED to indicate the system is starting up, LED functions will be handed over to the RGB led once initalized (the idea here is this led will remain on if the system crashes before the RGB led is initialized).

    // Configure RGB led.
    led_strip_config_t rgbLedConfig = {
        .strip_gpio_num = RGB_WS2812B_PIN,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false
        }
    };
    led_strip_rmt_config_t rmtConfig = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
        .mem_block_symbols = 0, // Let the driver decide the best memory block size.
        .flags = {
            .with_dma = false
        }
    };
    CHECK_ESP_RESULT(led_strip_new_rmt_device(&rgbLedConfig, &rmtConfig, &rgbLed));
    CHECK_ESP_RESULT(led_strip_set_pixel(rgbLed, 0, 0, 0, 0)); // Turn off the RGB LED at startup.
    CHECK_ESP_RESULT(led_strip_refresh(rgbLed));

    #ifdef LED_LOGGER
    static uint32_t lastLogTime = 0;
    static bool ledIsActive = false;
    static std::mutex ledMutex;

    xTaskCreate([](void* p)
    {
        while(true)
        {
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            {
                std::lock_guard<std::mutex> lock(ledMutex);
                if (ledIsActive && (now - lastLogTime > 100)) { // Xms timeout
                    led_strip_set_pixel(rgbLed, 0, 0, 0, 0);
                    led_strip_refresh(rgbLed);
                    ledIsActive = false;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(50)); // Check every 50ms
        }
    }, "LEDWatchdog", IDLE_TASK_STACK_SIZE + 128, NULL, tskIDLE_PRIORITY + 1, NULL);

    // Hook RGB led into the logger function.
    ReadieFur::Logging::AdditionalLoggers.push_back([](const char* data, size_t size, esp_log_level_t level) -> int {
        static esp_log_level_t lastLevel = ESP_LOG_NONE;

        bool r = false, g = false, b = false;
        switch (level)
        {
            case ESP_LOG_ERROR: r = true; break; // Red.
            case ESP_LOG_WARN: r = true; g = true; break; // Yellow.
            // case ESP_LOG_INFO: b = true; break; // Blue.
            // case ESP_LOG_DEBUG: r = true; b = true; break; // Purple.
            // case ESP_LOG_VERBOSE: g = true; break; // Green.
            default: return 0; // Don't change the LED for other log levels.
        }

        std::lock_guard<std::mutex> lock(ledMutex);
        lastLogTime = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // Only refresh if level has changed or LED was off.
        if (level != lastLevel || !ledIsActive) {
            const uint8_t l = 10; // Brightness (l = luminance)
            led_strip_set_pixel(rgbLed, 0, r ? l : 0, g ? l : 0, b ? l : 0);
            led_strip_refresh(rgbLed);
            ledIsActive = true;
            lastLevel = level;
        }

        return 0;
    });
    #endif

    gpio_set_level(STATIC_LED_PIN, 0); // Hand off LED control to the RGB led.
}

void InitFlash()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    CHECK_ESP_RESULT(err);
}

extern "C" void app_main()
{
    ConfigureLED();
    // SetCPUFrequency();
    SetLogLevel();
    InitFlash();

    // ==== Initialize Services ====

    CHECK_ESP_RESULT(Data::Flash::Init());
    CHECK_ESP_RESULT(Data::Persistent::Init());

    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<CAN::BusMaster>());

    #ifdef DEBUG
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<ReadieFur::Diagnostic::DiagnosticsService>());
    #endif

    #ifdef CAN_DUMP
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<CAN::BusLogger>());
    #endif

    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<Networking::BleApi>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<Networking::WiFiApi>());
    // CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<Networking::TCU>());
}
