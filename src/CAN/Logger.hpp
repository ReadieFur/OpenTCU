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
