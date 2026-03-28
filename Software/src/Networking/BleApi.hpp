#pragma once

#include "ProgramConfig.h"
#include <Service/AService.hpp>
#include <Network/Bluetooth/BLE.hpp>
#include <Network/Bluetooth/SGattServerProfile.h>
#include <Network/Bluetooth/GattServerService.hpp>
#include <esp_err.h>
#include <vector>
#include "CAN/BusMaster.hpp"
#include "Data/Persistent.hpp"
#include "Data/Live.hpp"
#include <string>
#include <cstring>

#define _BLE_TCU_NOTIFY(uuid, property) \
    esp_ble_gatts_send_indicate(_serverProfile.gattsIf, _serverProfile.connectionId, liveDataService.GetAttributeHandle(Network::Bluetooth::SUUID(uuid)), sizeof(CAN::SLiveData::property), reinterpret_cast<uint8_t*>(&liveData.property), false);

namespace ReadieFur::OpenTCU::Networking
{
    // https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Assigned_Numbers/out/en/Assigned_Numbers.pdf?v=1733673361043
    class BleApi : public Service::AService
    {
    private:
        std::vector<Network::Bluetooth::GattServerService*> _services;
        Network::Bluetooth::SGattServerProfile _serverProfile =
        {
            .appId = 0x01,
            .gattServerCallback = [this](auto a, auto b, auto c){ ServerAppCallback(a, b, c); },
        };

        void ServerAppCallback(esp_gatts_cb_event_t event, esp_gatt_if_t gattsIf, esp_ble_gatts_cb_param_t* param)
        {
            for (auto &&service : _services)
                service->ProcessServerEvent(event, gattsIf, param);
        }

    protected:
        void RunServiceImpl() override
        {
            // If the bike serial number is empty, wait for a short period to attempt to acquire it from the TCU before defaulting to using the MAC address as the device name.
            Data::Persistent::WaitForDeviceName(pdMS_TO_TICKS(5000));
            esp_err_t err = ReadieFur::Network::Bluetooth::BLE::Init(Data::Persistent::DeviceName.c_str(), Data::Persistent::Pin);
            if (err != ESP_OK)
            {
                LOGE(nameof(Networking::BleApi), "Failed to initialize BLE device: %s", esp_err_to_name(err));
                return;
            }

            CAN::BusMaster* busMaster = GetService<CAN::BusMaster>();

            Network::Bluetooth::GattServerService mainService(Network::Bluetooth::SUUID(0x29FCAA6CUL), 0);

            // Runtime stats.
            mainService.AddAttribute(
                Network::Bluetooth::SUUID(0xAD09C337UL),
                ESP_GATT_PERM_READ,
                [](uint8_t* outValue, uint16_t* outLength)
                {
                    memcpy(outValue, &Data::Live::Current, sizeof(Data::Live::Current));
                    *outLength = sizeof(Data::Live::Current);
                    return ESP_GATT_OK;
                });

            // Persistent data.
            mainService.AddAttribute(
                Network::Bluetooth::SUUID(0x3A3D3A3DUL),
                ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                [](uint8_t* outValue, uint16_t* outLength)
                {
                    *outLength = 0;

                    // TODO: Fix string parsing (possibly MTU size, default max is 20 bytes iirc).
                    // std::string stdStr = Data::Persistent::DeviceName.Get();
                    // const char* cStr = stdStr.c_str();
                    // size_t strLen = strlen(cStr);
                    // std::strncpy(reinterpret_cast<char*>(outValue), cStr, strLen);
                    // *outLength += strLen;
                    // outValue[*outLength++] = '\0';

                    // stdStr = Data::Persistent::BikeSerialNumber;
                    // cStr = stdStr.c_str();
                    // strLen = strlen(cStr);
                    // strncpy(reinterpret_cast<char*>(outValue + *outLength), cStr, strLen);
                    // *outLength += strLen;
                    // outValue[*outLength++] = '\0';

                    memcpy(outValue + *outLength, &Data::Persistent::BaseWheelCircumference, sizeof(Data::Persistent::BaseWheelCircumference));
                    *outLength += sizeof(Data::Persistent::BaseWheelCircumference);

                    memcpy(outValue + *outLength, &Data::Persistent::TargetWheelCircumference, sizeof(Data::Persistent::TargetWheelCircumference));
                    *outLength += sizeof(Data::Persistent::TargetWheelCircumference);

                    memcpy(outValue + *outLength, &Data::Persistent::Pin, sizeof(Data::Persistent::Pin));
                    *outLength += sizeof(Data::Persistent::Pin);
                    
                    return ESP_GATT_OK;
                },
                [this, busMaster](uint8_t* inValue, uint16_t inLength)
                {
                    size_t offset = 0;
                    size_t size;

                    // std::string deviceName(reinterpret_cast<char*>(inValue));
                    // offset += deviceName.length();

                    // Data::Persistent::BikeSerialNumber = reinterpret_cast<char*>(inValue + offset);
                    // offset += Data::Persistent::BikeSerialNumber.length();

                    size = sizeof(Data::Persistent::BaseWheelCircumference);
                    if (offset + size > inLength)
                        return ESP_GATT_ILLEGAL_PARAMETER;
                    uint16_t baseWheelCircumference = *(uint16_t*)(inValue + offset);
                    offset += size;
                    if (baseWheelCircumference > 2400 || baseWheelCircumference < 800)
                        return ESP_GATT_ILLEGAL_PARAMETER;

                    size = sizeof(Data::Persistent::TargetWheelCircumference);
                    if (offset + size > inLength)
                        return ESP_GATT_ILLEGAL_PARAMETER;
                    uint16_t targetWheelCircumference = *(uint16_t*)(inValue + offset);
                    offset += size;
                    if (targetWheelCircumference > 2400 || targetWheelCircumference < 800)
                        return ESP_GATT_ILLEGAL_PARAMETER;

                    size = sizeof(Data::Persistent::Pin);
                    if (offset + size > inLength)
                        return ESP_GATT_ILLEGAL_PARAMETER;
                    uint32_t pin = *(uint32_t*)(inValue + offset);
                    offset += size;

                    // Data::Persistent::DeviceName.Set(deviceName);
                    bool hasChanges = false;
                    if (Data::Persistent::BaseWheelCircumference != baseWheelCircumference)
                    {
                        LOGI(nameof(Networking::BleApi), "Setting base wheel circumference to %i", baseWheelCircumference);
                        Data::Persistent::BaseWheelCircumference = baseWheelCircumference;
                        hasChanges = true;
                    }
                    if (Data::Persistent::TargetWheelCircumference != targetWheelCircumference)
                    {
                        LOGI(nameof(Networking::BleApi), "Setting target wheel circumference to %i", targetWheelCircumference);
                        busMaster->SetTargetWheelCircumference(targetWheelCircumference); //The persistent data is updated via this call.
                        hasChanges = true;
                    }
                    if (Data::Persistent::Pin != pin)
                    {
                        LOGI(nameof(Networking::BleApi), "Setting new pin.");
                        Data::Persistent::Pin = pin;
                        ReadieFur::Network::Bluetooth::BLE::SetPin(pin);
                        hasChanges = true;
                    }

                    if (!hasChanges)
                        return ESP_GATT_OK;

                    Data::Persistent::Save();

                    return ESP_GATT_OK;
                });

            _services.push_back(&mainService);

            #ifdef DEBUG
            Network::Bluetooth::GattServerService debugService(Network::Bluetooth::SUUID(0x877C911DUL), 1);

            // CAN bus inject message.
            const size_t injectMessageDataSize =
                sizeof(uint8_t) //bus
                + sizeof(uint32_t) //id
                + sizeof(uint8_t) //length
                + (sizeof(uint8_t) * 8); //data
            debugService.AddAttribute(
                Network::Bluetooth::SUUID(0x78FDC1CEUL),
                ESP_GATT_PERM_WRITE,
                nullptr,
                [busMaster](uint8_t* inValue, uint16_t inLength)
                {
                    if (inLength != injectMessageDataSize)
                    {
                        LOGW(nameof(Networking::BleApi), "Invalid message length: %i", inLength);
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
                        LOGW(nameof(Networking::BleApi), "Invalid data length: %i", message.length);
                        return ESP_GATT_ILLEGAL_PARAMETER;
                    }
                    for (size_t i = 0; i < message.length; i++)
                        message.data[i] = inValue[6 + i];

                    esp_err_t res = busMaster->InjectMessage(bus, message);
                    if (res != ESP_OK)
                    {
                        LOGE(nameof(Networking::BleApi), "Failed to inject message: %i", res);
                        return ESP_GATT_INTERNAL_ERROR;
                    }

                    return ESP_GATT_OK;
                });

            // Reboot.
            debugService.AddAttribute(
                Network::Bluetooth::SUUID(0xBFB5E32FUL),
                ESP_GATT_PERM_WRITE,
                nullptr,
                [](uint8_t* inValue, uint16_t inLength)
                {
                    LOGW(nameof(Networking::BleApi), "Rebooting device.");
                    esp_restart();
                    return ESP_GATT_OK;
                }
            );

            _services.push_back(&debugService);
            #endif

            if ((err = Network::Bluetooth::BLE::RegisterServerApp(&_serverProfile)) != ESP_OK)
            {
                LOGE(nameof(Networking::BleApi), "Failed to register server app: %s", esp_err_to_name(err));
                return;
            }

            LOGD(nameof(Networking::BleApi), "BLE API started.");

            ServiceCancellationToken.WaitForCancellation();

            Network::Bluetooth::BLE::UnregisterServerApp(_serverProfile.appId);

            for (auto &&service : _services)
                delete service;
            _services.clear();
        }

    public:
        BleApi()
        {
            ServiceEntrypointStackDepth += 1024;
            AddDependencyType<CAN::BusMaster>();
            #ifdef CAN_DUMP
            AddDependencyType<CAN::BusLogger>();
            #endif
        }
    };
};
