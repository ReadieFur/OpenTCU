#pragma once

#include <stdint.h>

namespace ReadieFur::OpenTCU::CAN
{
    struct SLiveData
    {
        uint16_t BikeSpeed = 0;
        bool InWalkMode = false;
        uint8_t EaseSetting = 0;
        uint8_t PowerSetting = 0;
        uint16_t BatteryVoltage = 0;
        int32_t BatteryCurrent = 0;
    };
};
