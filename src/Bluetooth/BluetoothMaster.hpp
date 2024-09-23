#pragma once

#include "Abstractions/AService.hpp"
#include "Common.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLE2902.h>
#include <mutex>
#include "TCU.hpp"

static_assert(ESP_NAME[0] != '\0', "ESP_NAME cannot be empty.");

class BluetoothMaster : public AService
{
private:
    BLEServer* _server = nullptr;
    BLEClient* _client = nullptr;
    TCU* _tcu = nullptr;

protected:
    int InstallServiceImpl() override
    {
        BLEDevice::init(ESP_NAME);

        _server = BLEDevice::createServer(); 
        ESP_RETURN_ON_FALSE(_server != nullptr, 1, nameof(BluetoothMaster), "Failed to create BLE server.");
        _client = BLEDevice::createClient();
        ESP_RETURN_ON_FALSE(_client != nullptr, 2, nameof(BluetoothMaster), "Failed to create BLE client.");

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
