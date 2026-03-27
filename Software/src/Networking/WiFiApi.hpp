#pragma once

#include <Service/AService.hpp>
#include <esp_err.h>
#include "Logging.hpp"
#include <Network/WiFi/Modem.hpp>
#include <Network/WiFi/OTA.hpp>
#include "Data/PersistentData.hpp"
#include <string>
#include <cstring>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/inet.h>

namespace ReadieFur::OpenTCU::Networking
{
    class WiFiApi : public Service::AService
    {
    private:
        int UdpSocket;
        struct sockaddr_in UdpDestAddr;

        int LogUDP(const char* data, size_t length)
        {
            int udpErr = sendto(UdpSocket, data, length, 0, (struct sockaddr*)&UdpDestAddr, sizeof(UdpDestAddr));
            // if (udpErr < 0)
            //     LOGE(pcTaskGetName(NULL), "Failed to send UDP packet: %i", udpErr);
            return udpErr;
        }
        
    protected:
        void RunServiceImpl() override
        {
            //Configure AP.
            wifi_config_t apConfig =
            {
                .ap =
                {
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

            std::string deviceName = Data::PersistentData::DeviceName.Get(); //Returns a copy of the string which in testing gets mangles if not assigned to a variable before calling c_str().
            const char* deviceNameCStr = deviceName.c_str();
            apConfig.ap.ssid_len = strlen(deviceNameCStr);
            std::strncpy(reinterpret_cast<char*>(apConfig.ap.ssid), deviceNameCStr, sizeof(apConfig.ap.ssid));
            
            std::string password = "OpenTCU" + std::to_string(Data::PersistentData::Pin);
            const char* passwordCStr = password.c_str();
            std::strncpy(reinterpret_cast<char*>(apConfig.ap.password), passwordCStr, sizeof(apConfig.ap.password));

            esp_err_t err = ReadieFur::Network::WiFi::Modem::ConfigureInterface(WIFI_IF_AP, apConfig);
            if (err != ESP_OK)
            {
                LOGE(nameof(Networking::WiFiApi), "Failed to start AP mode: %s", esp_err_to_name(err));
                return;
            }

            // Configure UDP.
            UdpDestAddr.sin_addr.s_addr = inet_addr("192.168.4.255"); //Default broadcast address for the AP network.
            UdpDestAddr.sin_family = AF_INET;
            UdpDestAddr.sin_port = htons(49152);

            UdpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (UdpSocket < 0)
            {
                LOGE(nameof(Networking::WiFiApi), "Failed to create UDP socket.");
                return;
            }

            int udpBroadcastEnable = 1;
            setsockopt(UdpSocket, SOL_SOCKET, SO_BROADCAST, &udpBroadcastEnable, sizeof(udpBroadcastEnable));

            ReadieFur::Logging::AdditionalLoggers.push_back([this](const char* data, size_t length) { return LogUDP(data, length); });

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

            LOGI(nameof(Networking::WiFiApi), "AP mode started.");

            ServiceCancellationToken.WaitForCancellation();

            ReadieFur::Network::WiFi::OTA::Deinit();
            close(UdpSocket);
            ReadieFur::Network::WiFi::Modem::Deinit();
        }

    public:
        WiFiApi()
        {
            ServiceEntrypointStackDepth += 1024;
        }

        // int UDPSendRaw(const void* data, size_t length)
        // {
        //     return sendto(UdpSocket, data, length, 0, (struct sockaddr*)&UdpDestAddr, sizeof(UdpDestAddr));
        // }
    };
};
