#pragma once

#include "ACan.h"

namespace ReadieFur::OpenTCU::CAN
{
    struct SRelayTaskParameters
    {
        ACan* canA;
        ACan* canB;
    };
};
