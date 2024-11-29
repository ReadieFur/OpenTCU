#ifdef DEBUG
// #define _CAN_TEST
// #define ENABLE_CAN_DUMP_SERIAL
#define ENABLE_CAN_DUMP_UDP
#define LOG_UDP

#if defined(ENABLE_CAN_DUMP_SERIAL) || defined(ENABLE_CAN_DUMP_UDP)
#define ENABLE_CAN_DUMP
#endif
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
#include <Network/OTA/API.hpp>
#include <esp_pm.h>
#include "Bluetooth/TCU.hpp"
#include <string>
#include <esp_mac.h>
#include <cstring>
#ifdef LOG_UDP
#include <Network/WiFi.hpp>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/inet.h>
#endif

#define CHECK_SERVICE_RESULT(func) do {                                     \
        ReadieFur::Service::EServiceResult result = func;                   \
        if (result == ReadieFur::Service::Ok) break;                        \
        LOGE(pcTaskGetName(NULL), "Failed with result: %i", result);        \
        abort();                                                            \
    } while (0)

#define CHECK_ESP_RESULT(func) do {                                                     \
        esp_err_t result = func;                                                        \
        if (result == ESP_OK) break;                                                    \
        LOGE(pcTaskGetName(NULL), "Failed with result: %s", esp_err_to_name(result));   \
        abort();                                                                        \
    } while (0)

using namespace ReadieFur::OpenTCU;

std::string DeviceName = "OpenTCU" TCU_NAME; //Placeholder value.

#ifdef LOG_UDP
int UdpSocket;
struct sockaddr_in UdpDestAddr;
int UdpBroadcastEnable = 1;
#endif

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
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    //Set custom log levels.
    esp_log_level_set(nameof(CAN::BusMaster), ESP_LOG_WARN);
    esp_log_level_set(nameof(CAN::TwaiCan), ESP_LOG_ERROR);
    // esp_log_level_set(nameof(CAN::McpCan), ESP_LOG_ERROR);
    esp_log_level_set(nameof(CAN::Logger), ESP_LOG_INFO);
    esp_log_level_set(nameof(OTA::API), ESP_LOG_VERBOSE);
    // esp_log_level_set(nameof(Bluetooth::TCU), ESP_LOG_DEBUG);
    #else
    esp_log_level_set("*", ESP_LOG_INFO);
    #endif
}

void ConfigureDeviceName()
{
    //Get the device name based on the TCU ID.
    std::string deviceId = TCU_NAME;
    while (deviceId.length() > 0 && !isdigit(deviceId.front()))
        deviceId.erase(0, 1);
    if (deviceId.empty())
    {
        //Use the mac address as the device name.
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_BASE);
        deviceId = std::to_string(mac[0]) + std::to_string(mac[1]) + std::to_string(mac[2]) + std::to_string(mac[3]) + std::to_string(mac[4]) + std::to_string(mac[5]);
    }
    DeviceName = "OpenTCU" + deviceId;
}

    #endif
}

void setup()
{
    // SetCPUFrequency();
    SetLogLevel();

    #ifndef _CAN_TEST
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<CAN::BusMaster>());
    #else
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<CAN::Test>());
    #endif

    #ifdef DEBUG
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<ReadieFur::Diagnostic::DiagnosticsService>());
    #endif

    //Configure WiFi.
    CHECK_ESP_RESULT(ReadieFur::Network::WiFi::Init());
    wifi_config_t apConfig = {
        .ap = {
            .password = "OpenTCU" TCU_PIN, //Temporary, still left open for now.
            .ssid_len = (uint8_t)DeviceName.length(),
            .channel = 1,
            .authmode = WIFI_AUTH_OPEN,
            .ssid_hidden = 0,
            .max_connection = 4,
            .beacon_interval = 100,
        }
    };
    std::strncpy(reinterpret_cast<char*>(apConfig.ap.ssid), DeviceName.c_str(), sizeof(apConfig.ap.ssid));
    CHECK_ESP_RESULT(ReadieFur::Network::WiFi::ConfigureInterface(WIFI_IF_AP, apConfig));

    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<Bluetooth::TCU>());

    #ifdef ENABLE_CAN_DUMP
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<CAN::Logger>());
    #endif

    CHECK_ESP_RESULT(ReadieFur::Network::OTA::API::Init());
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
