#pragma once

#include <Service/AService.hpp>
#include "BusMaster.hpp"
#include <Helpers.h>
#include <Logging.hpp>
#include <string>
#ifdef ENABLE_CAN_DUMP_UDP
#include <Network/WiFi.hpp>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/inet.h>
#endif

namespace ReadieFur::OpenTCU::CAN
{
    class Logger : public Service::AService
    {
    private:
        static const TickType_t LOG_INTERVAL = pdMS_TO_TICKS(50);
        BusMaster* _busMaster = nullptr;

    protected:
        void RunServiceImpl() override
        {
            _busMaster = GetService<BusMaster>(); //Won't be null here, the service manager will ensure that all required services are started before this one.

            #ifdef ENABLE_CAN_DUMP_UDP
            if (!Network::WiFi::Initalized())
            {
                LOGE(nameof(CAN::Logger), "WiFi is not initialized.");
                return;
            }

            wifi_mode_t networkMode = Network::WiFi::GetMode();
            if (networkMode != WIFI_MODE_AP && networkMode != WIFI_MODE_APSTA)
            {
                LOGE(nameof(CAN::Logger), "WiFi is not in AP mode.");
                return;
            }

            int udpSocket;
            struct sockaddr_in udpDestAddr;

            udpDestAddr.sin_addr.s_addr = inet_addr("192.168.4.255"); //Default broadcast address for the ESP32 when running in AP mode.
            udpDestAddr.sin_family = AF_INET;
            udpDestAddr.sin_port = htons(49153);

            udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (udpSocket < 0)
            {
                LOGE(nameof(CAN::Logger), "Failed to create UDP socket.");
                return;
            }

            int udpBroadcastEnable = 1;
            setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, &udpBroadcastEnable, sizeof(udpBroadcastEnable));
            #endif

            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                //Process messages in batches.
                UBaseType_t capturedQueueLength = uxQueueMessagesWaiting(_busMaster->CanDumpQueue);
                // LOGD(nameof(CAN::Logger), "Captured %d messages.", capturedQueueLength);
                #if false
                if (capturedQueueLength == 0)
                {
                    std::string testData = "No CAN messages captured (" + std::to_string(esp_random()) + ")";
                    const char* testDataCStr = testData.c_str();
                    #ifdef ENABLE_CAN_DUMP_SERIAL
                    LOGI(nameof(CAN::Logger), "%s", testDataCStr);
                    #endif
                    #ifdef ENABLE_CAN_DUMP_UDP
                    if (sendto(udpSocket, testDataCStr, strlen(testDataCStr), 0, (struct sockaddr*)&udpDestAddr, sizeof(udpDestAddr)) < 0)
                        LOGE(nameof(CAN::Logger), "Failed to send UDP packet.");
                    #endif
                }
                #endif

                while (capturedQueueLength > 0 && uxQueueMessagesWaiting(_busMaster->CanDumpQueue) > 0)
                {
                    BusMaster::SCanDump dump;
                    if (xQueueReceive(_busMaster->CanDumpQueue, &dump, 0) != pdTRUE)
                        break;

                    ////It is faster to just dump all values rather than filter for only the amount of data sent, so instead we will post-process this data when it is not needed in real-time/is received on a more powerful device.
                    // char data[128];
                    // snprintf(data, sizeof(data), "%li,%c,%x,%d,%d,%d,%x,%x,%x,%x,%x,%x,%x,%x",
                    //     dump.timestamp,
                    //     dump.bus,
                    //     dump.message.id,
                    //     dump.message.isExtended,
                    //     dump.message.isRemote,
                    //     dump.message.length,
                    //     dump.message.data[0],
                    //     dump.message.data[1],
                    //     dump.message.data[2],
                    //     dump.message.data[3],
                    //     dump.message.data[4],
                    //     dump.message.data[5],
                    //     dump.message.data[6],
                    //     dump.message.data[7]);
                    std::string dataStr = std::to_string(dump.timestamp)
                    + "," + dump.bus
                    + "," + std::to_string(dump.message.id)
                    + "," + std::to_string(dump.message.isExtended)
                    + "," + std::to_string(dump.message.isRemote)
                    + "," + std::to_string(dump.message.length);
                    for (int i = 0; i < dump.message.length; i++)
                        dataStr += "," + std::to_string(dump.message.data[i]);
                    const char* data = dataStr.c_str();
                    dataStr.clear();

                    #ifdef ENABLE_CAN_DUMP_SERIAL
                    LOGI(nameof(CAN::Logger), "%s", data);
                    #endif

                    #ifdef ENABLE_CAN_DUMP_UDP
                    int udpErr = sendto(udpSocket, data, strlen(data), 0, (struct sockaddr*)&udpDestAddr, sizeof(udpDestAddr));
                    if (udpErr < 0)
                        LOGE(nameof(CAN::Logger), "Failed to send UDP packet: %i", udpErr);
                    #endif

                    capturedQueueLength--;
                    portYIELD();
                }

                vTaskDelay(LOG_INTERVAL);
            }

            #ifdef ENABLE_CAN_DUMP_UDP
            close(udpSocket);
            #endif

            _busMaster = nullptr;
        }

    public:
        Logger()
        {
            ServiceEntrypointStackDepth += 1024;
            AddDependencyType<BusMaster>();
        }
    };
};
