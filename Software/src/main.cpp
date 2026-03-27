#ifdef DEBUG
#define LOG_UDP
#define ENABLE_CAN_DUMP_SERIAL
#ifdef LOG_UDP
#define ENABLE_CAN_DUMP_UDP
#endif

#if defined(ENABLE_CAN_DUMP_SERIAL) || defined(ENABLE_CAN_DUMP_UDP)
#define ENABLE_CAN_DUMP
#endif
#endif

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
#include <Network/WiFi/Modem.hpp>
#include <Network/Wifi/OTA.hpp>
#include <esp_pm.h>
#include <Network/Bluetooth/BLE.hpp>
#include "Networking/BleApi.hpp"
#include "Networking/WiFiApi.hpp"
// #include "Networking/TCU.hpp"
#include <string>
#include <esp_mac.h>
#include <cstring>
#include "Data/Flash.hpp"
#include "Data/PersistentData.hpp"
#include <Event/Observable.hpp>
#include <Network/WiFi/EspNow.hpp> //TODO: Temporarily included to check compilation status of this file.

#ifdef WS2812B_PIN
#include <FastLED.h> //Currently not detected by the compiler...
CRGB leds[1];
#endif

//TODO: Change to take a status rather than colour.
void (*setLed)(ushort, ushort, ushort);

#define CHECK_SERVICE_RESULT(func) do {                                                 \
        ReadieFur::Service::EServiceResult result = func;                               \
        if (result == ReadieFur::Service::Ok) break;                                    \
        LOGE(pcTaskGetName(NULL), "[%d] Failed with result: %i", __LINE__, result);     \
        abort();                                                                        \
    } while (0)

#define CHECK_ESP_RESULT(func) do {                                                     \
        esp_err_t result = func;                                                        \
        if (result == ESP_OK) break;                                                    \
        LOGE(pcTaskGetName(NULL), "[%d] Failed with result: %s", __LINE__, esp_err_to_name(result));   \
        abort();                                                                        \
    } while (0)

using namespace ReadieFur::OpenTCU;

void SetCPUFrequency()
{
    //Set CPU frequency to the highest available as this real-time system needs to be as fast as possible.
    // esp_pm_config_t cpuConfig = {
    //     .max_freq_mhz = ESP_PM_CPU_FREQ_MAX,
    //     .min_freq_mhz = ESP_PM_CPU_FREQ_MAX,
    //     .light_sleep_enable = false
    // };
    // CHECK_ESP_RESULT(esp_pm_configure(&cpuConfig));
    //Setting the CPU frequency is not strictly required as when TWAI is enabled, the frequency is locked at ESP_PM_APB_FREQ_MAX as per the ESP-IDF documentation.
}

void SetLogLevel()
{
    #ifdef DEBUG
    //Set base log level.
    esp_log_level_set("*", ESP_LOG_DEBUG);
    //Set custom log levels.
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

extern "C" void app_main()
{
    #if defined(DEBUG) && false
    vTaskDelay(pdMS_TO_TICKS(2000)); //Delay to allow the serial port to initialize.
    #endif

    #ifdef _ENABLE_STDOUT_HOOK
    ReadieFur::Logging::OverrideStdout();
    #endif

    // SetCPUFrequency();
    SetLogLevel();

    gpio_config_t ledIOConfig = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    #if defined(WS2812B_PIN)
    setLed = [](ushort r, ushort g, ushort b) { leds[0] = CRGB(r, g, b); FastLED.show(); };
    ledIOConfig.pin_bit_mask = 1ULL << WS2812B_PIN;
    FastLED.addLeds<WS2812B, WS2812B_PIN, GRB>(leds, 1);
    #elif defined(LED_PIN)
    setLed = [](ushort r, ushort g, ushort b) { if (r > 0 || g > 0 || b > 0) gpio_set_level(LED_PIN, 1); else gpio_set_level(LED_PIN, 0); };
    ledIOConfig.pin_bit_mask = 1ULL << LED_PIN;
    #endif
    gpio_config(&ledIOConfig);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    CHECK_ESP_RESULT(err);

    CHECK_ESP_RESULT(Data::Flash::Init());
    CHECK_ESP_RESULT(Data::PersistentData::Init());
    // ReadieFur::Event::TObservableHandle deviceNameObserverHandle;
    // Data::PersistentData::DeviceName.Register(deviceNameObserverHandle);

    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<CAN::BusMaster>());
    // CAN::BusMaster* busMaster = ReadieFur::Service::ServiceManager::GetService<CAN::BusMaster>(); //Create a secondary watcher as we want to capture changes now but not wait on them until later.

    #ifdef DEBUG
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<ReadieFur::Diagnostic::DiagnosticsService>());
    #endif

    //Attempt to fetch the device serial number from the bus with some retries (in my testing it can take a few seconds between device boot and the serial number being automatically requested).
    //If the times out then the default value will be used.
    // Data::PersistentData::DeviceName.WaitOne(deviceNameObserverHandle, pdMS_TO_TICKS(3000));

    CHECK_ESP_RESULT(ReadieFur::Network::WiFi::Modem::Init());
    ReadieFur::Network::WiFi::Modem::ShutdownInterface(WIFI_IF_AP);

    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<Networking::BleApi>());
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<Networking::WiFiApi>());
    // CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<Networking::TCU>());
    // CHECK_ESP_RESULT(InitOTA()); //OTA currently configured in the BT API.

    #ifdef ENABLE_CAN_DUMP
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<CAN::Logger>());
    #endif

    CHECK_ESP_RESULT(ReadieFur::Network::WiFi::EspNow::Init()); //TODO: Move to own service file, just here for init testing.
}
