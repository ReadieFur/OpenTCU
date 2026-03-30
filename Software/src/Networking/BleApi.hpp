#pragma once

#include "ProgramConfig.h"
#include <Service/AService.hpp>
#include <esp_err.h>
#include <vector>
#include "CAN/BusMaster.hpp"
#include "Data/Persistent.hpp"
#include "Data/Live.hpp"
#include <string>
#include <cstring>

namespace ReadieFur::OpenTCU::Networking
{
    // https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Assigned_Numbers/out/en/Assigned_Numbers.pdf?v=1733673361043
    class BleApi : public Service::AService
    {
    private:

    protected:
        void RunServiceImpl() override
        {
            LOGD(nameof(Networking::BleApi), "BLE API started.");

            ServiceCancellationToken.WaitForCancellation();
        }

    public:
        BleApi()
        {
            ServiceEntrypointStackDepth += 1024;
            AddDependencyType<CAN::BusMaster>();
        }
    };
};
