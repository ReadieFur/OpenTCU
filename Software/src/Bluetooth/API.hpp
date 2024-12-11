#pragma once

#include <Service/AService.hpp>
#include <Network/Bluetooth/BLE.hpp>
#include <Network/Bluetooth/SGattServerProfile.h>
#include <Network/Bluetooth/GattServerService.hpp>
#include <esp_err.h>
#include <vector>
#include "CAN/BusMaster.hpp"
#include "CAN/SLiveData.h"
#ifdef ENABLE_CAN_DUMP
#include "CAN/Logger.hpp"
#endif

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
            #ifdef ENABLE_CAN_DUMP
            CAN::Logger* logger = GetService<CAN::Logger>();
            #endif

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

            #ifdef DEBUG
            Network::Bluetooth::GattServerService debugService(Network::Bluetooth::SUUID(0x877C911DUL), 1);

            //CAN bus inject message.
            const size_t injectMessageDataSize =
                sizeof(uint8_t) //bus
                + sizeof(uint32_t) //id
                + sizeof(uint8_t) //length
                + (sizeof(uint8_t) * 8); //data
            debugService.AddAttribute(
                Network::Bluetooth::SUUID(0x78FDC1CEUL),
                ESP_GATT_PERM_WRITE,
                0,
                nullptr,
                [busMaster](uint8_t* inValue, uint16_t inLength)
                {
                    if (inLength != injectMessageDataSize)
                    {
                        LOGW(nameof(Bluetooth::API), "Invalid message length: %i", inLength);
                        return ESP_GATT_ILLEGAL_PARAMETER;
                    }

                    bool bus = inValue[0];
                    CAN::SCanMessage message =
                    {
                        .id = (uint32_t)(inValue[1] | inValue[2] << 8 | inValue[3] << 16 | inValue[4] << 24),
                        .length = inValue[5],
                        .isExtended = false,
                        .isRemote = false
                    };
                    if (message.length > 8)
                    {
                        LOGW(nameof(Bluetooth::API), "Invalid data length: %i", message.length);
                        return ESP_GATT_ILLEGAL_PARAMETER;
                    }
                    for (size_t i = 0; i < message.length; i++)
                        message.data[i] = inValue[6 + i];

                    esp_err_t res = busMaster->InjectMessage(bus, message);
                    if (res != ESP_OK)
                    {
                        LOGE(nameof(Bluetooth::API), "Failed to inject message: %i", res);
                        return ESP_GATT_INTERNAL_ERROR;
                    }

                    return ESP_GATT_OK;
                });

            #ifdef ENABLE_CAN_DUMP
            //CAN bus logging whitelist.
            debugService.AddAttribute(
                Network::Bluetooth::SUUID(0x1450D8E6UL),
                ESP_GATT_PERM_WRITE,
                0,
                nullptr,
                [logger](uint8_t* inValue, uint16_t inLength)
                {
                    //Make sure the length is a multiple of 4.
                    if (inLength % 4 != 0)
                    {
                        LOGW(nameof(Bluetooth::API), "Invalid whitelist length: %i", inLength);
                        return ESP_GATT_ILLEGAL_PARAMETER;
                    }

                    LOGD(nameof(Bluetooth::API), "Clearing log whitelist.");
                    logger->Whitelist.clear();

                    for (size_t i = 0; i < inLength; i += 4)
                    {
                        uint32_t id = inValue[i] | inValue[i + 1] << 8 | inValue[i + 2] << 16 | inValue[i + 3] << 24;
                        LOGD(nameof(Bluetooth::API), "Adding ID to whitelist: %x", id);
                        logger->Whitelist.push_back(id);
                    }

                    return ESP_GATT_OK;
                });
            #endif

            //Reboot.
            debugService.AddAttribute(
                Network::Bluetooth::SUUID(0xBFB5E32FUL),
                ESP_GATT_PERM_WRITE,
                0,
                nullptr,
                [logger](uint8_t* inValue, uint16_t inLength)
                {
                    LOGW(nameof(Bluetooth::API), "Rebooting device.");
                    esp_restart();
                    return ESP_GATT_OK;
                }
            );

            //Toggle runtime CAN bus stats.
            debugService.AddAttribute(
                Network::Bluetooth::SUUID(0x9ED8266DUL),
                ESP_GATT_PERM_WRITE,
                0,
                nullptr,
                [busMaster](uint8_t* inValue, uint16_t inLength)
                {
                    busMaster->EnableRuntimeStats = !busMaster->EnableRuntimeStats;
                    return ESP_GATT_OK;
                }
            );

            _services.push_back(&debugService);
            #endif

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
            #ifdef ENABLE_CAN_DUMP
            AddDependencyType<CAN::Logger>();
            #endif
        }
    };
};
