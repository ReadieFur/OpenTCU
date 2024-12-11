#pragma once

#include <stdint.h>

namespace ReadieFur::OpenTCU::Data
{
    class RuntimeStats
    {
    public:
        static uint16_t BikeSpeed;
        static uint16_t RealSpeed;
        static uint16_t Cadence;
        static uint16_t RiderPower;
        static uint16_t MotorPower;
        static uint16_t BatteryVoltage;
        static uint32_t BatteryCurrent;
        static uint8_t EaseSetting;
        static uint8_t PowerSetting;
        static bool WalkMode;
    };
}

uint16_t ReadieFur::OpenTCU::Data::RuntimeStats::BikeSpeed = 0;
uint16_t ReadieFur::OpenTCU::Data::RuntimeStats::RealSpeed = 0;
uint16_t ReadieFur::OpenTCU::Data::RuntimeStats::Cadence = 0;
uint16_t ReadieFur::OpenTCU::Data::RuntimeStats::RiderPower = 0;
uint16_t ReadieFur::OpenTCU::Data::RuntimeStats::MotorPower = 0;
uint16_t ReadieFur::OpenTCU::Data::RuntimeStats::BatteryVoltage = 0;
uint32_t ReadieFur::OpenTCU::Data::RuntimeStats::BatteryCurrent = 0;
uint8_t ReadieFur::OpenTCU::Data::RuntimeStats::EaseSetting = 0;
uint8_t ReadieFur::OpenTCU::Data::RuntimeStats::PowerSetting = 0;
bool ReadieFur::OpenTCU::Data::RuntimeStats::WalkMode = false;
