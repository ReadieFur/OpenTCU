#pragma once

#include <Service/AService.hpp>
#include <Network/Bluetooth/BLE.hpp>
#include <Network/Bluetooth/SGattServerProfile.h>
#include <Network/Bluetooth/GattServerService.hpp>
#include <esp_err.h>
#include <vector>
#include "CAN/BusMaster.hpp"
#ifdef ENABLE_CAN_DUMP
#include "CAN/BusLogger.hpp"
#endif
#include "Data/PersistentData.hpp"
#include "Data/RuntimeStats.hpp"
#include <string>
#include <cstring>

#define _BLE_TCU_NOTIFY(uuid, property) \
    esp_ble_gatts_send_indicate(_serverProfile.gattsIf, _serverProfile.connectionId, liveDataService.GetAttributeHandle(Network::Bluetooth::SUUID(uuid)), sizeof(CAN::SLiveData::property), reinterpret_cast<uint8_t*>(&liveData.property), false);

namespace ReadieFur::OpenTCU::Networking
{
    //https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Assigned_Numbers/out/en/Assigned_Numbers.pdf?v=1733673361043
    class BleApi : public Service::AService
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
            esp_err_t err = ReadieFur::Network::Bluetooth::BLE::Init(Data::PersistentData::DeviceName.Get().c_str(), Data::PersistentData::Pin);
            if (err != ESP_OK)
            {
                LOGE(nameof(Networking::BleApi), "Failed to initialize BLE device: %s", esp_err_to_name(err));
                return;
            }

            CAN::BusMaster* busMaster = GetService<CAN::BusMaster>();
            #ifdef ENABLE_CAN_DUMP
            CAN::Logger* logger = GetService<CAN::Logger>();
            #endif

            //Temporary solution to sending the data.
            Network::Bluetooth::GattServerService mainService(Network::Bluetooth::SUUID(0x29FCAA6CUL), 0);

            //Runtime stats.
            mainService.AddAttribute(
                Network::Bluetooth::SUUID(0xAD09C337UL),
                ESP_GATT_PERM_READ,
                [busMaster](uint8_t* outValue, uint16_t* outLength)
                {
                    *outLength = 0;

                    memcpy(outValue + *outLength, &Data::RuntimeStats::BikeSpeed, sizeof(Data::RuntimeStats::BikeSpeed));
                    *outLength += sizeof(Data::RuntimeStats::BikeSpeed);

                    memcpy(outValue + *outLength, &Data::RuntimeStats::RealSpeed, sizeof(Data::RuntimeStats::RealSpeed));
                    *outLength += sizeof(Data::RuntimeStats::RealSpeed);

                    memcpy(outValue + *outLength, &Data::RuntimeStats::Cadence, sizeof(Data::RuntimeStats::Cadence));
                    *outLength += sizeof(Data::RuntimeStats::Cadence);

                    memcpy(outValue + *outLength, &Data::RuntimeStats::RiderPower, sizeof(Data::RuntimeStats::RiderPower));
                    *outLength += sizeof(Data::RuntimeStats::RiderPower);

                    memcpy(outValue + *outLength, &Data::RuntimeStats::MotorPower, sizeof(Data::RuntimeStats::MotorPower));
                    *outLength += sizeof(Data::RuntimeStats::MotorPower);

                    memcpy(outValue + *outLength, &Data::RuntimeStats::BatteryVoltage, sizeof(Data::RuntimeStats::BatteryVoltage));
                    *outLength += sizeof(Data::RuntimeStats::BatteryVoltage);

                    memcpy(outValue + *outLength, &Data::RuntimeStats::BatteryCurrent, sizeof(Data::RuntimeStats::BatteryCurrent));
                    *outLength += sizeof(Data::RuntimeStats::BatteryCurrent);

                    memcpy(outValue + *outLength, &Data::RuntimeStats::EaseSetting, sizeof(Data::RuntimeStats::EaseSetting));
                    *outLength += sizeof(Data::RuntimeStats::EaseSetting);

                    memcpy(outValue + *outLength, &Data::RuntimeStats::PowerSetting, sizeof(Data::RuntimeStats::PowerSetting));
                    *outLength += sizeof(Data::RuntimeStats::PowerSetting);

                    memcpy(outValue + *outLength, &Data::RuntimeStats::WalkMode, sizeof(Data::RuntimeStats::WalkMode));
                    *outLength += sizeof(Data::RuntimeStats::WalkMode);

                    return ESP_GATT_OK;
                });

            //Persistent data.
            mainService.AddAttribute(
                Network::Bluetooth::SUUID(0x3A3D3A3DUL),
                ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                [](uint8_t* outValue, uint16_t* outLength)
                {
                    *outLength = 0;

                    //TODO: Fix string parsing.
                    // std::string stdStr = Data::PersistentData::DeviceName.Get();
                    // const char* cStr = stdStr.c_str();
                    // size_t strLen = strlen(cStr);
                    // std::strncpy(reinterpret_cast<char*>(outValue), cStr, strLen);
                    // *outLength += strLen;
                    // outValue[*outLength++] = '\0';

                    // stdStr = Data::PersistentData::BikeSerialNumber;
                    // cStr = stdStr.c_str();
                    // strLen = strlen(cStr);
                    // strncpy(reinterpret_cast<char*>(outValue + *outLength), cStr, strLen);
                    // *outLength += strLen;
                    // outValue[*outLength++] = '\0';

                    memcpy(outValue + *outLength, &Data::PersistentData::BaseWheelCircumference, sizeof(Data::PersistentData::BaseWheelCircumference));
                    *outLength += sizeof(Data::PersistentData::BaseWheelCircumference);

                    memcpy(outValue + *outLength, &Data::PersistentData::TargetWheelCircumference, sizeof(Data::PersistentData::TargetWheelCircumference));
                    *outLength += sizeof(Data::PersistentData::TargetWheelCircumference);

                    memcpy(outValue + *outLength, &Data::PersistentData::Pin, sizeof(Data::PersistentData::Pin));
                    *outLength += sizeof(Data::PersistentData::Pin);
                    
                    return ESP_GATT_OK;
                },
                [this, busMaster](uint8_t* inValue, uint16_t inLength)
                {
                    size_t offset = 0;
                    size_t size;

                    // std::string deviceName(reinterpret_cast<char*>(inValue));
                    // offset += deviceName.length();

                    // Data::PersistentData::BikeSerialNumber = reinterpret_cast<char*>(inValue + offset);
                    // offset += Data::PersistentData::BikeSerialNumber.length();

                    size = sizeof(Data::PersistentData::BaseWheelCircumference);
                    if (offset + size > inLength)
                        return ESP_GATT_ILLEGAL_PARAMETER;
                    uint16_t baseWheelCircumference = *(uint16_t*)(inValue + offset);
                    offset += size;
                    if (baseWheelCircumference > 2400 || baseWheelCircumference < 800)
                        return ESP_GATT_ILLEGAL_PARAMETER;

                    size = sizeof(Data::PersistentData::TargetWheelCircumference);
                    if (offset + size > inLength)
                        return ESP_GATT_ILLEGAL_PARAMETER;
                    uint16_t targetWheelCircumference = *(uint16_t*)(inValue + offset);
                    offset += size;
                    if (targetWheelCircumference > 2400 || targetWheelCircumference < 800)
                        return ESP_GATT_ILLEGAL_PARAMETER;

                    size = sizeof(Data::PersistentData::Pin);
                    if (offset + size > inLength)
                        return ESP_GATT_ILLEGAL_PARAMETER;
                    uint32_t pin = *(uint32_t*)(inValue + offset);
                    offset += size;

                    // Data::PersistentData::DeviceName.Set(deviceName);
                    bool hasChanges = false;
                    if (Data::PersistentData::BaseWheelCircumference != baseWheelCircumference)
                    {
                        LOGI(nameof(Networking::BleApi), "Setting base wheel circumference to %i", baseWheelCircumference);
                        Data::PersistentData::BaseWheelCircumference = baseWheelCircumference;
                        hasChanges = true;
                    }
                    if (Data::PersistentData::TargetWheelCircumference != targetWheelCircumference)
                    {
                        LOGI(nameof(Networking::BleApi), "Setting target wheel circumference to %i", targetWheelCircumference);
                        busMaster->SetTargetWheelCircumference(targetWheelCircumference); //The persistent data is updated via this call.
                        hasChanges = true;
                    }
                    if (Data::PersistentData::Pin != pin)
                    {
                        LOGI(nameof(Networking::BleApi), "Setting new pin.");
                        Data::PersistentData::Pin = pin;
                        ReadieFur::Network::Bluetooth::BLE::SetPin(pin);
                        hasChanges = true;
                    }

                    if (!hasChanges)
                        return ESP_GATT_OK;

                    Data::PersistentData::Save();

                    return ESP_GATT_OK;
                });

            _services.push_back(&mainService);

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

            #ifdef ENABLE_CAN_DUMP
            //CAN bus logging whitelist.
            debugService.AddAttribute(
                Network::Bluetooth::SUUID(0x1450D8E6UL),
                ESP_GATT_PERM_WRITE,
                nullptr,
                [logger](uint8_t* inValue, uint16_t inLength)
                {
                    //Make sure the length is a multiple of 4.
                    if (inLength % 4 != 0)
                    {
                        LOGW(nameof(Networking::BleApi), "Invalid whitelist length: %i", inLength);
                        return ESP_GATT_ILLEGAL_PARAMETER;
                    }

                    LOGD(nameof(Networking::BleApi), "Clearing log whitelist.");
                    logger->Whitelist.clear();

                    for (size_t i = 0; i < inLength; i += 4)
                    {
                        uint32_t id = inValue[i] | inValue[i + 1] << 8 | inValue[i + 2] << 16 | inValue[i + 3] << 24;
                        LOGD(nameof(Networking::BleApi), "Adding ID to whitelist: %x", id);
                        logger->Whitelist.push_back(id);
                    }

                    return ESP_GATT_OK;
                });
            #endif

            //Reboot.
            debugService.AddAttribute(
                Network::Bluetooth::SUUID(0xBFB5E32FUL),
                ESP_GATT_PERM_WRITE,
                nullptr,
                [logger](uint8_t* inValue, uint16_t inLength)
                {
                    LOGW(nameof(Networking::BleApi), "Rebooting device.");
                    esp_restart();
                    return ESP_GATT_OK;
                }
            );

            //Toggle runtime CAN bus stats.
            debugService.AddAttribute(
                Network::Bluetooth::SUUID(0x9ED8266DUL),
                ESP_GATT_PERM_WRITE,
                nullptr,
                [busMaster](uint8_t* inValue, uint16_t inLength)
                {
                    LOGD(nameof(Networking::BleApi), "Toggling runtime stats to %s", busMaster->EnableRuntimeStats ? "off" : "on");
                    busMaster->EnableRuntimeStats = !busMaster->EnableRuntimeStats;
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
            #ifdef ENABLE_CAN_DUMP
            AddDependencyType<CAN::Logger>();
            #endif
        }
    };
};
