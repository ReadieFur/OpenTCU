#pragma once

#include <stdint.h>
#include <Event/Observable.hpp>
#include <string>
#include "Flash.hpp"
#include <Logging.hpp>
#include <ArduinoJson.h>
#include <esp_mac.h>

namespace ReadieFur::OpenTCU::Data
{
    class PersistentData
    {
    private:
        constexpr static const char* CONFIG_PATH = "/spiffs/persistent_data.json";

    public:
        static ReadieFur::Event::Observable<std::string> DeviceName;
        static std::string BikeSerialNumber; //TODO: Convert this to a service and wait on multiple properties for auto-saving.
        static uint16_t BaseWheelCircumference;
        static uint16_t TargetWheelCircumference;
        static uint32_t Pin;

        static esp_err_t Init()
        {
            SetDeviceNameFromBikeSerialNumber("");

            JsonDocument jsonDocument;
            esp_err_t err = Flash::LoadJson(CONFIG_PATH, jsonDocument);
            switch (err)
            {
            case ESP_OK:
                JSON_ASSIGN_TO_SOURCE_IF_TYPE(jsonDocument, BikeSerialNumber, std::string);
                JSON_ASSIGN_TO_SOURCE_IF_TYPE(jsonDocument, BaseWheelCircumference, uint16_t);
                JSON_ASSIGN_TO_SOURCE_IF_TYPE(jsonDocument, TargetWheelCircumference, uint16_t);
                JSON_ASSIGN_TO_SOURCE_IF_TYPE(jsonDocument, Pin, uint32_t);
                SetDeviceNameFromBikeSerialNumber(BikeSerialNumber);
                return ESP_OK;
            case ESP_ERR_NOT_FOUND:
                //Use the default config.
                return ESP_OK;
            case ESP_FAIL:
                LOGE(nameof(PersistentData), "Failed to load persistent data (%s). Using the default config for this session.", esp_err_to_name(err));
                return ESP_OK;
            default:
                LOGE(nameof(PersistentData), "Failed to load persistent data: %s", esp_err_to_name(err));
                return err;
            }
        }

        static esp_err_t Save()
        {
            JsonDocument jsonDocument;
            JSON_SET_PROP(jsonDocument, BikeSerialNumber);
            JSON_SET_PROP(jsonDocument, BaseWheelCircumference);
            JSON_SET_PROP(jsonDocument, TargetWheelCircumference);
            JSON_SET_PROP(jsonDocument, Pin);
            return Flash::SaveJson(CONFIG_PATH, jsonDocument);
        }

        static void SetDeviceNameFromBikeSerialNumber(std::string bikeSerialNumber)
        {
            //Get the device name based on the TCU ID.
            while (bikeSerialNumber.length() > 0 && !isdigit(bikeSerialNumber.front()))
                bikeSerialNumber.erase(0, 1);
            if (bikeSerialNumber.empty())
            {
                //Use the mac address as the device name.
                uint8_t mac[6];
                esp_read_mac(mac, ESP_MAC_BASE);
                bikeSerialNumber = std::to_string(mac[0]) + std::to_string(mac[1]) + std::to_string(mac[2]) + std::to_string(mac[3]) + std::to_string(mac[4]) + std::to_string(mac[5]);
            }
            DeviceName.Set("OpenTCU" + bikeSerialNumber);
            LOGD(nameof(PersistentData), "Device name set to: %s", DeviceName.Get().c_str());
        }
    };
};

ReadieFur::Event::Observable<std::string> ReadieFur::OpenTCU::Data::PersistentData::DeviceName("OpenTCU");
std::string ReadieFur::OpenTCU::Data::PersistentData::BikeSerialNumber;
uint16_t ReadieFur::OpenTCU::Data::PersistentData::BaseWheelCircumference = 2160;
uint16_t ReadieFur::OpenTCU::Data::PersistentData::TargetWheelCircumference = 2160;
uint32_t ReadieFur::OpenTCU::Data::PersistentData::Pin;
