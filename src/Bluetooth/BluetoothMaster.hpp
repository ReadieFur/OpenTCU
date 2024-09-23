#pragma once

#include "Abstractions/AService.hpp"
#include "Common.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <mutex>

static_assert(ESP_NAME[0] != '\0', "ESP_NAME cannot be empty.");

class BluetoothMaster : public AService
{
private:
    BLEServer* _server = nullptr;
    BLEClient* _client = nullptr;

protected:
    int InstallServiceImpl() override
    {
        BLEDevice::init(ESP_NAME);

        _server = BLEDevice::createServer();     
        _client = BLEDevice::createClient();

        return 0;
    }

    int UninstallServiceImpl() override
    {
        delete _server;
        delete _client;
        return 0;
    }

    int StartServiceImpl() override
    {
        BLEDevice::getAdvertising()->start();
        return 0;
    }

    int StopServiceImpl() override
    {
        BLEDevice::stopAdvertising();
        return 0;
    }

public:
};
