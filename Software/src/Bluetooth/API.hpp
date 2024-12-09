#pragma once

#include <Service/AService.hpp>
#include <Network/Bluetooth/BLE.hpp>
#include <Network/Bluetooth/SGattServerProfile.h>
#include <Network/Bluetooth/GattServerService.hpp>
#include <esp_err.h>
#include <vector>
#include "CAN/BusMaster.hpp"
#include "CAN/SLiveData.h"

#define _BLE_TCU_ATTR(uuid, property) \
    liveDataService.AddAttribute(Network::Bluetooth::SUUID(uuid), ESP_GATT_PERM_READ, sizeof(CAN::SLiveData::property), \
    [&liveData](uint8_t* outValue, uint16_t* outLength) { \
        *reinterpret_cast<uint16_t*>(outValue) = liveData.property; \
        *outLength = sizeof(CAN::SLiveData::property); \
        return ESP_GATT_OK; \
    })

#define _BLE_TCU_NOTIFY(uuid, property) \
    esp_ble_gatts_send_indicate(_serverProfile.gattsIf, _serverProfile.connectionId, liveDataService.GetAttributeHandle(Network::Bluetooth::SUUID(uuid)), sizeof(CAN::SLiveData::property), reinterpret_cast<uint8_t*>(&liveData.property), false);

namespace ReadieFur::OpenTCU::Bluetooth
{
    //https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Assigned_Numbers/out/en/Assigned_Numbers.pdf?v=1733673361043
    class API : public Service::AService
    {
    private:
        Network::Bluetooth::SGattServerProfile _serverProfile =
        {
            .appId = 0x01,
            .gattServerCallback = [this](auto a, auto b, auto c){ ServerAppCallback(a, b, c); },
        };

        std::vector<Network::Bluetooth::GattServerService*> _services;

        void ServerAppCallback(esp_gatts_cb_event_t event, esp_gatt_if_t gattsIf, esp_ble_gatts_cb_param_t* param)
        {
            for (auto &&service : _services)
                service->ProcessServerEvent(event, gattsIf, param);
        }

    protected:
        void RunServiceImpl() override
        {
            if (!Network::Bluetooth::BLE::IsInitialized())
            {
                LOGE(nameof(Bluetooth::API), "BLE API not initialized.");
                return;
            }

            CAN::BusMaster* busMaster = GetService<CAN::BusMaster>();
            CAN::SLiveData liveData = {};

            //Temporary solution to sending the data.
            Network::Bluetooth::GattServerService liveDataService(Network::Bluetooth::SUUID(0x29FCAA6CUL), 0);
            _BLE_TCU_ATTR(0xAD09C337UL, BikeSpeed);
            _BLE_TCU_ATTR(0x42F3D9B0UL, InWalkMode);
            _BLE_TCU_ATTR(0xB79F6C66UL, EaseSetting);
            _BLE_TCU_ATTR(0x117BC871UL, PowerSetting);
            _BLE_TCU_ATTR(0x79AAC18DUL, BatteryVoltage);
            _BLE_TCU_ATTR(0x741A2984UL, BatteryCurrent);
            _services.push_back(&liveDataService);

            esp_err_t err;
            if ((err = Network::Bluetooth::BLE::RegisterServerApp(&_serverProfile)) != ESP_OK)
            {
                LOGE(nameof(Bluetooth::API), "Failed to register server app: %s", esp_err_to_name(err));
                return;
            }

            LOGD(nameof(Bluetooth::API), "BLE API started.");

            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                liveData = busMaster->GetLiveData();
                // if (_serverProfile.connectionId != 0)
                // {
                //     _BLE_TCU_NOTIFY(0xAD09C337UL, BikeSpeed);
                //     _BLE_TCU_NOTIFY(0x42F3D9B0UL, InWalkMode);
                //     _BLE_TCU_NOTIFY(0xB79F6C66UL, EaseSetting);
                //     _BLE_TCU_NOTIFY(0x117BC871UL, PowerSetting);
                //     _BLE_TCU_NOTIFY(0x79AAC18DUL, BatteryVoltage);
                //     _BLE_TCU_NOTIFY(0x741A2984UL, BatteryCurrent);
                // }
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            Network::Bluetooth::BLE::UnregisterServerApp(_serverProfile.appId);

            for (auto &&service : _services)
                delete service;
            _services.clear();
        }

    public:
        API()
        {
            ServiceEntrypointStackDepth += 1024;
            AddDependencyType<CAN::BusMaster>();
        }
    };
};
