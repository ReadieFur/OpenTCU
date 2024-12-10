#ifdef DEBUG
// #define LOG_UDP
#define ENABLE_CAN_DUMP_SERIAL
#ifdef LOG_UDP
// #define ENABLE_CAN_DUMP_UDP
#endif

#if defined(ENABLE_CAN_DUMP_SERIAL) || defined(ENABLE_CAN_DUMP_UDP)
#define ENABLE_CAN_DUMP
#endif
#endif

#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include "Service/ServiceManager.hpp"
#include "Config/Device.h"
#include "CAN/BusMaster.hpp"
#include "CAN/Logger.hpp"
#include <esp_sleep.h>
#include <freertos/task.h>
#include "Logging.hpp"
#ifdef DEBUG
#include "Diagnostic/DiagnosticsService.hpp"
#endif
#include <Network/WiFi.hpp>
#include <Network/OTA/API.hpp>
#include <esp_pm.h>
#include <Network/Bluetooth/BLE.hpp>
#include "Bluetooth/API.hpp"
// #include "Bluetooth/TCU.hpp"
#include <string>
#include <esp_mac.h>
#include <cstring>
#ifdef LOG_UDP
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/inet.h>
#endif
#include "Config/Flash.hpp"

#define CHECK_SERVICE_RESULT(func) do {                                                 \
        ReadieFur::Service::EServiceResult result = func;                               \
        if (result == ReadieFur::Service::Ok) break;                                    \
        LOGE(pcTaskGetName(NULL), "[%d] Failed with result: %i", __LINE__, result);                    \
        abort();                                                                        \
    } while (0)

#define CHECK_ESP_RESULT(func) do {                                                     \
        esp_err_t result = func;                                                        \
        if (result == ESP_OK) break;                                                    \
        LOGE(pcTaskGetName(NULL), "[%d] Failed with result: %s", __LINE__, esp_err_to_name(result));   \
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
    esp_log_level_set("*", ESP_LOG_DEBUG);
    //Set custom log levels.
    esp_log_level_set(nameof(CAN::BusMaster), ESP_LOG_DEBUG);
    esp_log_level_set(nameof(CAN::TwaiCan), ESP_LOG_ERROR);
    // esp_log_level_set(nameof(CAN::McpCan), ESP_LOG_ERROR);
    esp_log_level_set(nameof(CAN::Logger), ESP_LOG_INFO);
    esp_log_level_set(nameof(OTA::API), ESP_LOG_DEBUG);
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

#ifdef LOG_UDP
int LogUDP(const char* message, size_t length)
{
    int udpErr = sendto(UdpSocket, message, length, 0, (struct sockaddr*)&UdpDestAddr, sizeof(UdpDestAddr));
    // if (udpErr < 0)
    //     LOGE(pcTaskGetName(NULL), "Failed to send UDP packet: %i", udpErr);
    return udpErr;
}
#endif

void ConfigureAdditionalLoggers()
{
    #ifdef LOG_UDP
    if (!ReadieFur::Network::WiFi::Initalized())
    {
        LOGE(nameof(CAN::Logger), "WiFi is not initialized.");
        return;
    }

    wifi_mode_t networkMode = ReadieFur::Network::WiFi::GetMode();
    if (networkMode != WIFI_MODE_AP && networkMode != WIFI_MODE_APSTA)
    {
        LOGE(nameof(CAN::Logger), "WiFi is not in AP mode.");
        return;
    }

    UdpDestAddr.sin_addr.s_addr = inet_addr("192.168.4.255"); //Default broadcast address for the AP network.
    UdpDestAddr.sin_family = AF_INET;
    UdpDestAddr.sin_port = htons(49152);

    UdpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (UdpSocket < 0)
    {
        LOGE(nameof(CAN::Logger), "Failed to create UDP socket.");
        return;
    }

    setsockopt(UdpSocket, SOL_SOCKET, SO_BROADCAST, &UdpBroadcastEnable, sizeof(UdpBroadcastEnable));

    ReadieFur::Logging::AdditionalLoggers.push_back(LogUDP);
    #endif
}

extern "C" void app_main()
{
    #ifdef _ENABLE_STDOUT_HOOK
    ReadieFur::Logging::OverrideStdout();
    #endif

    // SetCPUFrequency();
    SetLogLevel();

    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<CAN::BusMaster>());

    #ifdef DEBUG
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<ReadieFur::Diagnostic::DiagnosticsService>());
    #endif

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    CHECK_ESP_RESULT(err);


    ConfigureDeviceName();

    CHECK_ESP_RESULT(ReadieFur::Network::WiFi::Init());
    wifi_config_t apConfig =
    {
        .ap =
        {
            // .password = "OpenTCU" TCU_PIN, //Temporary, still left open for now.
            .ssid_len = (uint8_t)DeviceName.length(),
            .channel = 1,
            #ifdef DEBUG
            .authmode = WIFI_AUTH_OPEN,
            .ssid_hidden = 0,
            #else
            .authmode = WIFI_AUTH_WPA2_PSK,
            .ssid_hidden = 1,
            #endif
            .max_connection = 2,
            .beacon_interval = 100,
        }
    };
    std::strncpy(reinterpret_cast<char*>(apConfig.ap.ssid), DeviceName.c_str(), sizeof(apConfig.ap.ssid));
    CHECK_ESP_RESULT(ReadieFur::Network::WiFi::ConfigureInterface(WIFI_IF_AP, apConfig));

    #ifdef DEBUG
    ConfigureAdditionalLoggers();
    #endif

    #ifdef ENABLE_CAN_DUMP
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<CAN::Logger>());
    #ifdef LOG_UDP
    ReadieFur::Service::ServiceManager::GetService<CAN::Logger>()->UdpLogger = LogUDP;
    #endif
    #endif

    CHECK_ESP_RESULT(ReadieFur::Network::Bluetooth::BLE::Init(DeviceName.c_str(), TCU_PIN));
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<Bluetooth::API>());
    // CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<Bluetooth::TCU>());

    httpd_config_t otaHttpdConfig = HTTPD_DEFAULT_CONFIG();
    otaHttpdConfig.server_port = 81;
    otaHttpdConfig.ctrl_port += 1;
    CHECK_ESP_RESULT(ReadieFur::Network::OTA::API::Init(&otaHttpdConfig));
}
