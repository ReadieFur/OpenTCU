#pragma once

#include "Abstractions/AService.hpp"
#include "BluetoothMaster.hpp"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

static_assert(TCU_PIN[0] != '\0', "ESP_NAME cannot be empty.");

namespace ReadieFur::OpenTCU::Bluetooth
{
    class BleApi : public Abstractions::AService, BLECharacteristicCallbacks
    {
    private:
        const char* SERVICE_UUID = "d62c896f-5cb9-4aac-a0e7-b10c42d8ab04";
        const char* AUTH_CHARACTERISTIC = "f5679a85-9e5b-4411-bf08-9d298e74f6e4";

        BLEService* _service = nullptr;
        //From what I can see the ESP32 does not support PIN code pairing for bluetooth, so I might instead take a bearer approach with the API calls.
        BLECharacteristic* _authCharacteristic = nullptr;
        ulong gattEventCallbackID = -1;
        bool isClientAuthorized = false;

        void GattEventCallback(esp_ble_gatts_cb_param_t* param)
        {
            if (GetDependency<BluetoothMaster>()->IsClientConnectedToServer())
            {
                //Clear the auth status.
                _authCharacteristic->setValue("INVALID");
                isClientAuthorized = false;
                //Notify the client about the updated characteristic value.
                _authCharacteristic->notify();
            }
        }

        void HandleAuthCharacteristic()
        {
            std::string receivedPin = _authCharacteristic->getValue();

            if (receivedPin != TCU_PIN)
            {
                _authCharacteristic->setValue("INVALID");
                isClientAuthorized = false;
                _authCharacteristic->notify();
                return;
            }

            _authCharacteristic->setValue("OK");
            isClientAuthorized = true;
            _authCharacteristic->notify();

            //TODO: Enable API features.
        }

    protected:
        int InstallServiceImpl() override
        {
            _service = GetDependency<BluetoothMaster>()->_server->createService(SERVICE_UUID);
            _authCharacteristic = _service->createCharacteristic(AUTH_CHARACTERISTIC,
                BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
            _authCharacteristic->setCallbacks(this);

            return 0;
        }

        int UninstallServiceImpl() override
        {
            GetDependency<BluetoothMaster>()->_server->removeService(_service);
            return 0;
        }

        int StartServiceImpl() override
        {
            gattEventCallbackID = GetDependency<BluetoothMaster>()->OnGattEvent.Add([this](esp_ble_gatts_cb_param_t* param){ GattEventCallback(param); });
            _service->start();
            return 0;
        }

        int StopServiceImpl() override
        {
            GetDependency<BluetoothMaster>()->OnGattEvent.Remove(gattEventCallbackID);
            _service->stop();
            return 0;
        }
    
    public:
        BleApi()
        {
            AddDependencyType<BluetoothMaster>();
        }

        void onWrite(BLECharacteristic* _characteristic) override
        {
            if (_characteristic == _authCharacteristic)
                HandleAuthCharacteristic();
        }
    };
};
