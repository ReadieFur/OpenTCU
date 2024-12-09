#pragma once

namespace ReadieFur::OpenTCU::CAN
{
    enum EStringType
    {
        None = 0,
        MotorSerialNumber = 0x02,
        MotorHardwareID = 0x03,
        BikeSerialNumber = 0x04,
    };
};
