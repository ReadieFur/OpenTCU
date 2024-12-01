#pragma once

#include <Service/AService.hpp>
#include "BusMaster.hpp"
#include <Helpers.h>
#include <Logging.hpp>
#include <string>
#include <functional>

namespace ReadieFur::OpenTCU::CAN
{
    class Logger : public Service::AService
    {
    private:
        static const TickType_t LOG_INTERVAL = pdMS_TO_TICKS(500);
        BusMaster* _busMaster = nullptr;

        inline void SendLog(const char* format, ...)
        {
            va_list args;
            va_start(args, format);
            char buffer[256];
            vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);

            #ifdef ENABLE_CAN_DUMP_SERIAL
            // LOGI(nameof(CAN::Logger), "%s", buffer);
            fputs(buffer, stdout);
            #endif

            #ifdef ENABLE_CAN_DUMP_UDP
            if (UdpLogger != nullptr)
                UdpLogger(buffer, strlen(buffer));
            #endif
        }

        #ifdef ENABLE_CAN_DUMP
        inline void Log(BusMaster::SCanDump& dump)
        {
            //Doing this the long way because previous dynamic methods were causing issues.
            switch (dump.message.length)
            {
                //Shouldn't be 0, if it is, dump all data just in case.
                case 1:
                    SendLog(nameof(CAN::Logger)":%lu,%u,%u,%u,%u,%u,%u",
                        dump.timestamp,
                        dump.bus,
                        dump.message.id,
                        dump.message.isExtended,
                        dump.message.isRemote,
                        dump.message.length,
                        dump.message.data[0]);
                    break;
                case 2:
                    SendLog(nameof(CAN::Logger)":%lu,%u,%u,%u,%u,%u,%u,%u",
                        dump.timestamp,
                        dump.bus,
                        dump.message.id,
                        dump.message.isExtended,
                        dump.message.isRemote,
                        dump.message.length,
                        dump.message.data[0],
                        dump.message.data[1]);
                    break;
                case 3:
                    SendLog(nameof(CAN::Logger)":%lu,%u,%u,%u,%u,%u,%u,%u,%u",
                        dump.timestamp,
                        dump.bus,
                        dump.message.id,
                        dump.message.isExtended,
                        dump.message.isRemote,
                        dump.message.length,
                        dump.message.data[0],
                        dump.message.data[1],
                        dump.message.data[2]);
                    break;
                case 4:
                    SendLog(nameof(CAN::Logger)":%lu,%u,%u,%u,%u,%u,%u,%u,%u,%u",
                        dump.timestamp,
                        dump.bus,
                        dump.message.id,
                        dump.message.isExtended,
                        dump.message.isRemote,
                        dump.message.length,
                        dump.message.data[0],
                        dump.message.data[1],
                        dump.message.data[2],
                        dump.message.data[3]);
                    break;
                case 5:
                    SendLog(nameof(CAN::Logger)":%lu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
                        dump.timestamp,
                        dump.bus,
                        dump.message.id,
                        dump.message.isExtended,
                        dump.message.isRemote,
                        dump.message.length,
                        dump.message.data[0],
                        dump.message.data[1],
                        dump.message.data[2],
                        dump.message.data[3],
                        dump.message.data[4]);
                    break;
                case 6:
                    SendLog(nameof(CAN::Logger)":%lu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
                        dump.timestamp,
                        dump.bus,
                        dump.message.id,
                        dump.message.isExtended,
                        dump.message.isRemote,
                        dump.message.length,
                        dump.message.data[0],
                        dump.message.data[1],
                        dump.message.data[2],
                        dump.message.data[3],
                        dump.message.data[4],
                        dump.message.data[5]);
                    break;
                case 7:
                    SendLog(nameof(CAN::Logger)":%lu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
                        dump.timestamp,
                        dump.bus,
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
                        dump.message.data[6]);
                    break;
                default:
                    SendLog(nameof(CAN::Logger)":%lu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
                        dump.timestamp,
                        dump.bus,
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
                        dump.message.data[7]);
                    break;
            }
        }
        #endif

    protected:
        void RunServiceImpl() override
        {
            _busMaster = GetService<BusMaster>(); //Won't be null here, the service manager will ensure that all required services are started before this one.

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

            _busMaster = nullptr;
        }

    public:
        std::function<int(const char*, size_t)> UdpLogger = nullptr;

        Logger()
        {
            ServiceEntrypointStackDepth += 1024;
            AddDependencyType<BusMaster>();
        }
    };
};
