#pragma once

#include <Service/AService.hpp>
#include "BusMaster.hpp"
#include "Networking/WiFiApi.hpp"
#include <Helpers.h>
#include <Logging.hpp>
#include <string>
#include <functional>
#include <vector>

namespace ReadieFur::OpenTCU::CAN
{
    class Logger : public Service::AService
    {
    public:
        std::vector<uint32_t> Whitelist;

    private:
        static const TickType_t LOG_INTERVAL = pdMS_TO_TICKS(500);
        BusMaster* _busMaster = nullptr;
        Networking::WiFiApi* _wifiApi = nullptr;
        int UdpSocket;
        struct sockaddr_in UdpDestAddr;

        std::vector<uint32_t> _recognisedIds;

        #ifdef ENABLE_CAN_DUMP_SERIAL
        inline void SerialLog(const char* format, ...)
        {
            va_list args;
            va_start(args, format);
            char buffer[256];
            vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);

            puts(buffer);
        }
        #endif

        #ifdef ENABLE_CAN_DUMP
        inline void Log(BusMaster::SCanDump& dump)
        {
            if (std::find(_recognisedIds.begin(), _recognisedIds.end(), dump.id) == _recognisedIds.end())
            {
                LOGI(nameof(CAN::Logger), "New ID detected: %x", dump.id);
                _recognisedIds.push_back(dump.id);

                // //Add new IDs to the whitelist so they aren't missed.
                // if (Whitelist.size() > 0)
                //     Whitelist.push_back(dump.id);
            }

            if (Whitelist.size() > 0 && std::find(Whitelist.begin(), Whitelist.end(), dump.id) == Whitelist.end())
                return;

            int bus = (char)dump.bus == '1' ? 0 : 1;

            #ifdef ENABLE_CAN_DUMP_SERIAL
            char hex_buffer[(8 * 3) + 1]; //8 bytes * 3 chars each "XX," + null
            char* p = hex_buffer;
            for (int i = 0; i < dump.length; i++)
                p += sprintf(p, (i < dump.length - 1) ? "%02X," : "%02X", dump.data[i]);

            SerialLog(nameof(CAN::Logger)":%lu,%u,%x,%u,%u,%u,%s",
                    dump.timestamp,
                    bus,
                    dump.id,
                    dump.isExtended,
                    dump.isRemote,
                    dump.length,
                    hex_buffer);
            #endif

            #ifdef ENABLE_CAN_DUMP_UDP
            int bytesSent = sendto(UdpSocket, &dump, sizeof(dump), 0, (struct sockaddr*)&UdpDestAddr, sizeof(UdpDestAddr));
            // if (bytesSent < 0)
            //     LOGE(nameof(CAN::Logger), "Failed to send UDP packet: %i", bytesSent);
            #endif
        }
        #endif

    protected:
        void RunServiceImpl() override
        {
            // Get dependencies.
            _busMaster = GetService<BusMaster>(); //Won't be null here, the service manager will ensure that all required services are started before this one.
            _wifiApi = GetService<Networking::WiFiApi>();

            // Configure UDP for CAN dump.
            // AP should always be ready here due to the dependency.
            UdpDestAddr.sin_addr.s_addr = inet_addr("192.168.4.255");
            UdpDestAddr.sin_family = AF_INET;
            UdpDestAddr.sin_port = htons(49153);

            UdpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (UdpSocket < 0)
            {
                LOGE(nameof(Networking::WiFiApi), "Failed to create UDP socket.");
                return;
            }

            fcntl(UdpSocket, F_SETFL, O_NONBLOCK); //Set socket to non-blocking to prevent potential issues with the logging task.

            int udpBroadcastEnable = 1;
            setsockopt(UdpSocket, SOL_SOCKET, SO_BROADCAST, &udpBroadcastEnable, sizeof(udpBroadcastEnable));

            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                #ifdef ENABLE_CAN_DUMP
                #if defined(_LIVE_LOG) && false
                //Process messages as they come in.
                BusMaster::SCanDump dump;
                if (xQueueReceive(_busMaster->CanDumpQueue, &dump, portMAX_DELAY) == pdTRUE)
                    Log(dump);
                #else
                //Process messages in batches.
                UBaseType_t capturedQueueLength = uxQueueMessagesWaiting(_busMaster->CanDumpQueue);
                while (capturedQueueLength > 0 && uxQueueMessagesWaiting(_busMaster->CanDumpQueue) > 0)
                {
                    BusMaster::SCanDump dump;
                    if (xQueueReceive(_busMaster->CanDumpQueue, &dump, 0) != pdTRUE)
                        break;
                    Log(dump);
                    capturedQueueLength--;
                    portYIELD();
                }
                vTaskDelay(LOG_INTERVAL);
                #endif
                #endif
            }

            close(UdpSocket);
            _busMaster = nullptr;
        }

    public:
        Logger()
        {
            ServiceEntrypointStackDepth += 1024;
            AddDependencyType<BusMaster>();
            AddDependencyType<Networking::WiFiApi>();
        }
    };
};
