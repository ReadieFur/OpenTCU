#pragma once

#include <BLEServer.h>

namespace ReadieFur::OpenTCU::Bluetooth
{
    class API
    {
    private:
        BLEServer& _server;

    public:
        API(BLEServer& server) : _server(server) {}
    };
};
