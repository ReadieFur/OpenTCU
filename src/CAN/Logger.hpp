#pragma once

#include <Service/AService.hpp>
#include "BusMaster.hpp"
#include <Helpers.h>
#include <Logging.hpp>
#include <string>

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

            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                //Process messages in batches.
                UBaseType_t capturedQueueLength = uxQueueMessagesWaiting(_busMaster->CanDumpQueue);
                while (capturedQueueLength > 0 && uxQueueMessagesWaiting(_busMaster->CanDumpQueue) > 0)
                {
                    BusMaster::SCanDump dump;
                    if (xQueueReceive(_busMaster->CanDumpQueue, &dump, 0) != pdTRUE)
                        break;

                    #if false
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
                    #else
                    //The above wasn't working with output so I am going to do it the longer snprintf way.
                    char data[256];
                    switch (dump.message.length)
                    {
                    case 1:
                        snprintf(data, sizeof(data), "%lu,%u,%u,%u,%u,%u,%u",
                            dump.timestamp,
                            dump.bus,
                            dump.message.id,
                            dump.message.isExtended,
                            dump.message.isRemote,
                            dump.message.length,
                            dump.message.data[0]);
                        break;
                    case 2:
                        snprintf(data, sizeof(data), "%lu,%u,%u,%u,%u,%u,%u,%u",
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
                        snprintf(data, sizeof(data), "%lu,%u,%u,%u,%u,%u,%u,%u,%u",
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
                        snprintf(data, sizeof(data), "%lu,%u,%u,%u,%u,%u,%u,%u,%u,%u",
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
                        snprintf(data, sizeof(data), "%lu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
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
                        snprintf(data, sizeof(data), "%lu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
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
                        snprintf(data, sizeof(data), "%lu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
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
                        snprintf(data, sizeof(data), "%lu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
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
                    #endif

                    #ifdef ENABLE_CAN_DUMP
                    LOGI(nameof(CAN::Logger), "%s", data);
                    #endif

                    capturedQueueLength--;
                    portYIELD();
                }

                vTaskDelay(LOG_INTERVAL);
            }

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
