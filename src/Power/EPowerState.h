#pragma once

namespace ReadieFur::OpenTCU::Power
{
    enum EPowerState
    {
        PluggedIn,
        BatteryNormal,
        BatteryLow,
        BatteryCritical
    };
};
