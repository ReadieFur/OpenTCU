#pragma once

#include <stdint.h>
#include <esp_gatts_api.h>
#include <functional>

namespace ReadieFur::OpenTCU::Bluetooth
{
    struct SGattServerProfile
    {
        uint16_t appId;
        std::function<void(esp_gatts_cb_event_t, esp_gatt_if_t, esp_ble_gatts_cb_param_t*)> gattServerCallback = nullptr;
        uint16_t gattsIf = ESP_GATT_IF_NONE;
        uint16_t connectionId;
        uint16_t serviceHandle;
        esp_gatt_srvc_id_t serviceId;
        uint16_t charHandle;
        esp_bt_uuid_t charUuid;
        esp_gatt_perm_t permissions;
        esp_gatt_char_prop_t property;
        uint16_t descriptorHandle;
        esp_bt_uuid_t descriptorUuid;
    };
};
