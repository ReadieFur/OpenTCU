#pragma once

#include <stdint.h>

namespace ReadieFur::OpenTCU::Data
{
    struct __attribute__((packed)) SLive
    {
        uint16_t BikeSpeed = 0;
        uint16_t RealSpeed = 0;
        uint16_t Cadence = 0;
        uint16_t RiderPower = 0;
        uint16_t MotorPower = 0;
        uint16_t BatteryVoltage = 0;
        uint32_t BatteryCurrent = 0;
        uint8_t EaseSetting = 0;
        uint8_t PowerSetting = 0;
        bool WalkMode = false;
    };

    class Live
    {
    public:
        static SLive Current;
    };
}

ReadieFur::OpenTCU::Data::SLive ReadieFur::OpenTCU::Data::Live::Current;
