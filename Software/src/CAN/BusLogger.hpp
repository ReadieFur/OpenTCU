#pragma once

#include "ProgramConfig.h"
#include <Service/AService.hpp>
#include "BusMaster.hpp"
#include <Helpers.h>
#include <Logging.hpp>
#include <string>
#include <functional>
#include <vector>

namespace ReadieFur::OpenTCU::CAN
{
    class BusLogger : public Service::AService
    {
    public:
        std::function<int(const void* data, size_t length)> UDPSendFunc = nullptr; // Dynamically set by the WiFiApi service to avoid circular dependency issues.

    private:
        static const TickType_t LOG_INTERVAL = pdMS_TO_TICKS(500);
        BusMaster* _busMaster = nullptr;

        #ifdef CAN_DUMP_SERIAL
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

        #ifdef CAN_DUMP
        inline void Log(BusMaster::SCanDump& dump)
        {
            #ifdef CAN_DUMP_SERIAL
            char hex_buffer[(8 * 3) + 1]; // 8 bytes * 3 chars each "XX," + null
            char* p = hex_buffer;
            for (int i = 0; i < dump.length; i++)
                p += sprintf(p, (i < dump.length - 1) ? "%02X," : "%02X", dump.data[i]);

            SerialLog(nameof(CAN::Logger)":%lu,%u,%x,%u,%u,%u,%s",
                    dump.timestamp,
                    dump.bus,
                    dump.id,
                    dump.isExtended,
                    dump.isRemote,
                    dump.length,
                    hex_buffer);
            #endif

            #ifdef CAN_DUMP_UDP
            if (UDPSendFunc != nullptr)
            {
                int bytesSent = UDPSendFunc(&dump, sizeof(dump));
                // if (bytesSent < 0)
                //     LOGE(nameof(CAN::Logger), "Failed to send UDP packet: %i", bytesSent);
            }
            #endif
        }
        #endif

    protected:
        void RunServiceImpl() override
        {
            // Get dependencies.
            _busMaster = GetService<BusMaster>(); // Won't be null here, the service manager will ensure that all required services are started before this one.

            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                #ifdef CAN_DUMP_LIVE
                // Process messages as they come in.
                BusMaster::SCanDump dump;
                if (xQueueReceive(_busMaster->CanDumpQueue, &dump, portMAX_DELAY) == pdTRUE)
                    Log(dump);
                #else
                // Process messages in batches.
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
            }

            _busMaster = nullptr;
        }

    public:
        BusLogger()
        {
            ServiceEntrypointStackDepth += 1024;
            AddDependencyType<BusMaster>();
        }
    };
};
