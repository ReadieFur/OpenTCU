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
                #ifdef ENABLE_CAN_DUMP
                //Only process messages in batches.
                UBaseType_t capturedQueueLength = uxQueueMessagesWaiting(_busMaster->CanDumpQueue);
                LOGD(nameof(Logger), "Captured %d messages.", capturedQueueLength);
                while (capturedQueueLength > 0 && uxQueueMessagesWaiting(_busMaster->CanDumpQueue) > 0)
                {
                    BusMaster::SCanDump dump;
                    if (xQueueReceive(_busMaster->CanDumpQueue, &dump, 0) != pdTRUE)
                        break;

                    //It is faster to just dump all values rather than filter for only the amount of data sent, so instead we will post-process this data when it is not needed in real-time/is received on a more powerful device.
                    LOGI(nameof(Logger), "%li,%c,%x,%d,%d,%d,%x,%x,%x,%x,%x,%x,%x,%x",
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

                    capturedQueueLength--;
                    portYIELD();
                }
                #endif

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
