#pragma once

#include <BLEClient.h>

namespace ReadieFur::OpenTCU::Bluetooth
{
    class TCU
    {
    private:
        BLEClient& _client;

    public:
        TCU(BLEClient& client) : _client(client) {}
    };
};
