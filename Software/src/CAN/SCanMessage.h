#pragma once

#include <stdint.h>

namespace ReadieFur::OpenTCU::CAN
{
    struct SCanMessage
    {
        uint32_t id;
        uint8_t data[8];
        uint8_t length;
        bool isExtended;
        bool isRemote;
    };
};
