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
#include "Data/Flash.hpp"
#include "Data/PersistentData.hpp"
#include <Event/Observable.hpp>

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

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    CHECK_ESP_RESULT(err);

    CHECK_ESP_RESULT(Data::Flash::Init());
    CHECK_ESP_RESULT(Data::PersistentData::Init());
    ReadieFur::Event::TObservableHandle deviceNameObserverHandle;
    Data::PersistentData::DeviceName.Register(deviceNameObserverHandle);

    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<CAN::BusMaster>());
    CAN::BusMaster* busMaster = ReadieFur::Service::ServiceManager::GetService<CAN::BusMaster>(); //Create a secondary watcher as we want to capture changes now but not wait on them until later.

    #ifdef DEBUG
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<ReadieFur::Diagnostic::DiagnosticsService>());
    #endif

    //Attempt to fetch the device serial number from the bus with some retries (in my testing it can take a few seconds between device boot and the serial number being automatically requested).
    //If the times out then the default value will be used.
    // Data::PersistentData::DeviceName.WaitOne(deviceNameObserverHandle, pdMS_TO_TICKS(3000));

    CHECK_ESP_RESULT(ReadieFur::Network::WiFi::Init());

    #ifdef DEBUG
    ConfigureAdditionalLoggers();
    #endif

    #ifdef ENABLE_CAN_DUMP
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<CAN::Logger>());
    #ifdef LOG_UDP
    ReadieFur::Service::ServiceManager::GetService<CAN::Logger>()->UdpLogger = LogUDP;
    #endif
    #endif

    CHECK_ESP_RESULT(ReadieFur::Network::Bluetooth::BLE::Init(Data::PersistentData::DeviceName.Get().c_str(), Data::PersistentData::Pin));
    CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<Bluetooth::API>());
    // CHECK_SERVICE_RESULT(ReadieFur::Service::ServiceManager::InstallAndStartService<Bluetooth::TCU>());
}
