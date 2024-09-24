#pragma once

#include "Abstractions/AService.hpp"
#include "pch.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLE2902.h>
#include <mutex>
#include "Abstractions/Event.hpp"

static_assert(TCU_NAME[0] != '\0', "ESP_NAME cannot be empty.");

namespace ReadieFur::OpenTCU::Bluetooth
{
    class BluetoothMaster : public Abstractions::AService, BLEServerCallbacks
    {
    private:
        bool clientConnectedToServer = false;

    protected:
        int InstallServiceImpl() override
        {
            //Strip the string prefix from the TCU name and replace it with ESP.
            String deviceName = TCU_NAME;
            while (deviceName.length() > 0 && !isdigit(deviceName[0]))
                deviceName = deviceName.substring(1);
            if (deviceName.isEmpty())
                deviceName = TCU_NAME;
            deviceName = "ESP" + deviceName;

            BLEDevice::init(TCU_NAME);

            _server = BLEDevice::createServer(); 
            // ESP_RETURN_ON_FALSE(_server != nullptr, 1, nameof(BluetoothMaster), "Failed to create BLE server.");
            // _client = BLEDevice::createClient();
            // ESP_RETURN_ON_FALSE(_client != nullptr, 2, nameof(BluetoothMaster), "Failed to create BLE client.");

            return 0;
        }

        int UninstallServiceImpl() override
        {
            delete _server;
            // delete _client;
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
        BLEServer* _server = nullptr;
        // BLEClient* _client = nullptr;
        Abstractions::Event<esp_ble_gatts_cb_param_t*> OnGattEvent;

        void onConnect(BLEServer* server, esp_ble_gatts_cb_param_t* param) override
        {
            clientConnectedToServer = true;
            OnGattEvent.Dispatch(param);
        }

        void onDisconnect(BLEServer* server, esp_ble_gatts_cb_param_t* param) override
        {
            clientConnectedToServer = false;
            OnGattEvent.Dispatch(param);
        }

        // void onMtuChanged(BLEServer* pServer, esp_ble_gatts_cb_param_t* param) override
        // {
        //     OnGattEvent.Dispatch(param);
        // }

        bool IsClientConnectedToServer()
        {
            return clientConnectedToServer;
        }
    };
};
