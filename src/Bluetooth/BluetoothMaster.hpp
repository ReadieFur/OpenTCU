#pragma once

#include "StaticConfig.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <mutex>

class BluetoothMaster
{
private:
    static std::mutex _mutex;
    static bool _initalized;
    static BLEServer* _server;
    static BLEClient* _client;

public:
    static void Begin()
    {
        _mutex.lock();
        if (_initalized)
        {
            _mutex.unlock();
            return;
        }

        BLEDevice::init(ESP_NAME);

        _server = BLEDevice::createServer();        
        BLEDevice::getAdvertising()->start();

        _client = BLEDevice::createClient();

        _mutex.unlock();
    }

    static void End()
    {
        _mutex.lock();
        if (!_initalized)
        {
            _mutex.unlock();
            return;
        }

        BLEDevice::stopAdvertising();
        delete _server;

        _mutex.unlock();
    }
};

std::mutex BluetoothMaster::_mutex;
BLEServer* BluetoothMaster::_server = nullptr;
BLEClient* BluetoothMaster::_client = nullptr;
