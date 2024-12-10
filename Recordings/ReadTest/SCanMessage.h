#pragma once

#include <stdint.h>

struct SCanMessage
{
    bool bus;
    uint32_t id;
    uint8_t data[8];
    uint8_t length;
    bool isExtended;
    bool isRemote;
};
