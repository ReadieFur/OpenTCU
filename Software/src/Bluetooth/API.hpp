#pragma once

#include <Service/AService.hpp>
#include <Network/Bluetooth/BLE.hpp>
#include <Network/Bluetooth/SGattServerProfile.h>
#include <Network/Bluetooth/GattServerService.hpp>
#include <esp_err.h>
#include <vector>
#include "CAN/BusMaster.hpp"
#ifdef ENABLE_CAN_DUMP
#include "CAN/Logger.hpp"
#endif
#include "Data/PersistentData.hpp"
#include "Data/RuntimeStats.hpp"
#include <Network/WiFi.hpp>
#include <string>
#include <cstring>

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

        esp_gatt_status_t ConfigureAP()
        {
            //If the AP is already active, reconfigure it as this call may have be made with updated settings.

            wifi_config_t apConfig =
            {
                .ap =
                {
                    .channel = 1,
                    #ifdef DEBUG
                    .authmode = WIFI_AUTH_OPEN,
                    .ssid_hidden = 0,
                    #else
                    .authmode = WIFI_AUTH_WPA2_PSK,
                    .ssid_hidden = 1,
                    #endif
                    .max_connection = 2,
                    .beacon_interval = 100,
                }
            };

            std::string deviceName = Data::PersistentData::DeviceName.Get(); //Returns a copy of the string which in testing gets mangles if not assigned to a variable before calling c_str().
            const char* deviceNameCStr = deviceName.c_str();
            apConfig.ap.ssid_len = strlen(deviceNameCStr);
            std::strncpy(reinterpret_cast<char*>(apConfig.ap.ssid), deviceNameCStr, sizeof(apConfig.ap.ssid));
            
            std::string password = "OpenTCU" + std::to_string(Data::PersistentData::Pin);
            const char* passwordCStr = password.c_str();
            std::strncpy(reinterpret_cast<char*>(apConfig.ap.password), passwordCStr, sizeof(apConfig.ap.password));

            esp_err_t err = ReadieFur::Network::WiFi::ConfigureInterface(WIFI_IF_AP, apConfig);
            if (err != ESP_OK)
            {
                LOGE(nameof(Bluetooth::API), "Failed to start AP mode: %s", esp_err_to_name(err));
                return ESP_GATT_INTERNAL_ERROR;
            }

            httpd_config_t otaHttpdConfig = HTTPD_DEFAULT_CONFIG();
            otaHttpdConfig.server_port = 81;
            otaHttpdConfig.ctrl_port += 1;
            err = ReadieFur::Network::OTA::API::Init(&otaHttpdConfig);
            if (err != ESP_OK)
            {
                LOGE(nameof(Bluetooth::API), "Failed to start OTA server: %s", esp_err_to_name(err));
                return ESP_GATT_INTERNAL_ERROR;
            }

            LOGI(nameof(Bluetooth::API), "AP mode started.");
            return ESP_GATT_OK;
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

                    std::string deviceName = Data::PersistentData::DeviceName.Get();
                    std::strncpy(reinterpret_cast<char*>(outValue), deviceName.c_str(), deviceName.length());
                    *outLength += deviceName.length();
                    outValue[*outLength++] = '\0';

                    strncpy(reinterpret_cast<char*>(outValue + *outLength), Data::PersistentData::BikeSerialNumber.c_str(), Data::PersistentData::BikeSerialNumber.length());
                    *outLength += Data::PersistentData::BikeSerialNumber.length();
                    outValue[*outLength++] = '\0';

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
                        Data::PersistentData::BaseWheelCircumference = baseWheelCircumference;
                        hasChanges = true;
                    }
                    if (Data::PersistentData::TargetWheelCircumference != targetWheelCircumference)
                    {
                        busMaster->SetTargetWheelCircumference(targetWheelCircumference); //The persistent data is updated via this call.
                        hasChanges = true;
                    }
                    if (Data::PersistentData::Pin != pin)
                    {
                        Data::PersistentData::Pin = pin;
                        ReadieFur::Network::Bluetooth::BLE::SetPin(pin);
                        hasChanges = true;
                    }

                    if (!hasChanges)
                        return ESP_GATT_OK;

                    if (ReadieFur::Network::WiFi::GetMode() == WIFI_MODE_AP)
                        return ConfigureAP();

                    Data::PersistentData::Save();

                    return ESP_GATT_OK;
                });

            //AP toggle.
            mainService.AddAttribute(
                Network::Bluetooth::SUUID(0xB45D9CEDUL),
                ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                [](uint8_t* outValue, uint16_t* outLength)
                {
                    outValue[0] = ReadieFur::Network::WiFi::GetMode() == WIFI_MODE_AP ? 0x01 : 0x00;
                    *outLength = sizeof(uint8_t);
                    return ESP_GATT_OK;
                },
                [this](uint8_t* inValue, uint16_t inLength)
                {
                    if (inLength != sizeof(uint8_t))
                        return ESP_GATT_ILLEGAL_PARAMETER;

                    LOGD(nameof(Bluetooth::API), "Setting AP mode to %s", inValue[0] == 0x01 ? "on" : "off");

                    //TODO: Fix this.
                    bool enable = inValue[0] == 0x01;
                    wifi_mode_t currentMode = ReadieFur::Network::WiFi::GetMode();
                    if (enable && currentMode == WIFI_MODE_AP)
                    {
                        LOGD(nameof(Bluetooth::API), "AP mode is already enabled.");
                        return ESP_GATT_OK;
                    }
                    else if (!enable && currentMode != WIFI_MODE_AP)
                    {
                        LOGD(nameof(Bluetooth::API), "AP mode is already disabled.");
                        return ESP_GATT_OK;
                    }
                    else if (enable && currentMode != WIFI_MODE_AP)
                    {
                        return ConfigureAP();
                    }
                    else if (!enable && currentMode == WIFI_MODE_AP)
                    {
                        ReadieFur::Network::OTA::API::Deinit();
                        esp_err_t err = ReadieFur::Network::WiFi::ShutdownInterface(WIFI_IF_AP);
                        if (err != ESP_OK)
                        {
                            LOGE(nameof(Bluetooth::API), "Failed to stop AP mode: %s", esp_err_to_name(err));
                            return ESP_GATT_INTERNAL_ERROR;
                        }

                        LOGI(nameof(Bluetooth::API), "AP mode stopped.");
                        return ESP_GATT_OK;
                    }

                    LOGE(nameof(Bluetooth::API), "Invalid AP mode state.");
                    return ESP_GATT_INTERNAL_ERROR; //We shouldn't reach here.
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
                nullptr,
                [busMaster](uint8_t* inValue, uint16_t inLength)
                {
                    LOGD(nameof(Bluetooth::API), "Toggling runtime stats to %s", busMaster->EnableRuntimeStats ? "off" : "on");
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

            ServiceCancellationToken.WaitForCancellation();

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
