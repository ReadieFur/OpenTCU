#pragma once

#include "ProgramConfig.h"
#include <Service/AService.hpp>
#include <esp_err.h>
#include "Logging.hpp"
#include <Network/WiFi/Modem.hpp>
#include <Network/WiFi/OTA.hpp>
#include "CAN/BusLogger.hpp"
#include "Data/Persistent.hpp"
#include <string>
#include <cstring>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/inet.h>

#define __UDP_BROADCAST_ADDRESS "192.168.4.255" // Default broadcast address for the AP network.

namespace ReadieFur::OpenTCU::Networking
{
    class WiFiApi : public Service::AService
    {
    private:
        int _udpLoggerSocket;
        struct sockaddr_in _udpLoggerDest;
        #ifdef CAN_DUMP
        int _udpBusSocket;
        struct sockaddr_in _udpBusDest;
        #endif

        int LogUDP(const char* data, size_t length)
        {
            int udpErr = sendto(_udpLoggerSocket, data, length, 0, (struct sockaddr*)&_udpLoggerDest, sizeof(_udpLoggerDest));
            // if (udpErr < 0)
            //     LOGE(pcTaskGetName(NULL), "Failed to send UDP packet: %i", udpErr);
            return udpErr;
        }
        
    protected:
        void RunServiceImpl() override
        {
            esp_err_t err = ReadieFur::Network::WiFi::Modem::Init();
            if (err != ESP_OK)
            {
                LOGE(nameof(Networking::WiFiApi), "Failed to initialize Wi-Fi modem: %s", esp_err_to_name(err));
                return;
            }
            ReadieFur::Network::WiFi::Modem::ShutdownInterface(WIFI_IF_STA);

            // Configure AP.
            wifi_config_t apConfig =
            {
                .ap =
                {
                    .channel = 1,
                    #ifdef DEBUG
                    .authmode = WIFI_AUTH_OPEN,
                    .ssid_hidden = 0,
                    #else
                    // .authmode = WIFI_AUTH_WPA2_PSK,
                    .authmode = WIFI_AUTH_OPEN,
                    .ssid_hidden = 1,
                    #endif
                    .max_connection = 2,
                    .beacon_interval = 100,
                }
            };

            Data::Persistent::WaitForDeviceName(pdMS_TO_TICKS(5000));
            std::string deviceName = Data::Persistent::DeviceName; // Returns a copy of the string which in testing gets mangles if not assigned to a variable before calling c_str().
            const char* deviceNameCStr = deviceName.c_str();
            apConfig.ap.ssid_len = strlen(deviceNameCStr);
            std::strncpy(reinterpret_cast<char*>(apConfig.ap.ssid), deviceNameCStr, sizeof(apConfig.ap.ssid));
            
            std::string password = "OpenTCU" + std::to_string(Data::Persistent::Pin);
            const char* passwordCStr = password.c_str();
            std::strncpy(reinterpret_cast<char*>(apConfig.ap.password), passwordCStr, sizeof(apConfig.ap.password));

            err = ReadieFur::Network::WiFi::Modem::ConfigureInterface(WIFI_IF_AP, apConfig);
            if (err != ESP_OK)
            {
                LOGE(nameof(Networking::WiFiApi), "Failed to start AP mode: %s", esp_err_to_name(err));
                return;
            }

            // Configure UDP Logger.
            _udpLoggerDest.sin_addr.s_addr = inet_addr(__UDP_BROADCAST_ADDRESS);
            _udpLoggerDest.sin_family = AF_INET;
            _udpLoggerDest.sin_port = htons(49152);
            _udpLoggerSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (_udpLoggerSocket < 0)
            {
                LOGE(nameof(Networking::WiFiApi), "Failed to create UDP socket.");
                return;
            }
            int udpBroadcastEnable = 1;
            setsockopt(_udpLoggerSocket, SOL_SOCKET, SO_BROADCAST, &udpBroadcastEnable, sizeof(udpBroadcastEnable));
            ReadieFur::Logging::AdditionalLoggers.push_back([this](const char* data, size_t length, esp_log_level_t level) { return LogUDP(data, length); });

            #ifdef CAN_DUMP
            CAN::BusLogger* busLogger = GetService<CAN::BusLogger>();
            // Dynamically fetch service (prevents circular dependency issues).
            if (busLogger != nullptr)
            {
                // Configure UDP for CAN dump.
                _udpBusDest.sin_addr.s_addr = inet_addr(__UDP_BROADCAST_ADDRESS);
                _udpBusDest.sin_family = AF_INET;
                _udpBusDest.sin_port = htons(49153);
                _udpBusSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                if (_udpBusSocket < 0)
                {
                    LOGE(nameof(Networking::WiFiApi), "Failed to create UDP socket.");
                    return;
                }
                fcntl(_udpBusSocket, F_SETFL, O_NONBLOCK); // Set socket to non-blocking to prevent potential issues with the logging task.
                setsockopt(_udpBusSocket, SOL_SOCKET, SO_BROADCAST, &udpBroadcastEnable, sizeof(udpBroadcastEnable));
            
                busLogger->UDPSendFunc = [this](const void* data, size_t length) { return LogUDP(reinterpret_cast<const char*>(data), length); };
            }
            #endif

            // Configure OTA.
            httpd_config_t otaHttpdConfig = HTTPD_DEFAULT_CONFIG();
            otaHttpdConfig.task_priority = tskIDLE_PRIORITY + 5;
            otaHttpdConfig.server_port = 81;
            otaHttpdConfig.ctrl_port += 1;
            err = ReadieFur::Network::WiFi::OTA::Init(&otaHttpdConfig);
            if (err != ESP_OK)
            {
                LOGE(nameof(Networking::WiFiApi), "Failed to start OTA server: %s", esp_err_to_name(err));
                return;
            }

            LOGI(nameof(Networking::WiFiApi), "WiFi service configured.");

            // CHECK_ESP_RESULT(ReadieFur::Network::WiFi::EspNow::Init());

            ServiceCancellationToken.WaitForCancellation();

            ReadieFur::Network::WiFi::OTA::Deinit();
            if (busLogger != nullptr)
                busLogger->UDPSendFunc = nullptr;
            close(_udpLoggerSocket);
            ReadieFur::Network::WiFi::Modem::Deinit();
        }

    public:
        WiFiApi()
        {
            ServiceEntrypointStackDepth += 1024 * 2;
            #ifdef CAN_DUMP
            AddDependencyType<CAN::BusLogger>(); // Undoes the need for the GetService check above but including it anyway.
            #endif
        }

        // int UDPSendRaw(const void* data, size_t length)
        // {
        //     return sendto(_udpLoggerSocket, data, length, 0, (struct sockaddr*)&_udpLoggerDest, sizeof(_udpLoggerDest));
        // }
    };
};
