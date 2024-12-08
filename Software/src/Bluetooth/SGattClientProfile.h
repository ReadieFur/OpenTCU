#pragma once

#include <stdint.h>
#include <esp_gattc_api.h>
#include <functional>

namespace ReadieFur::OpenTCU::Bluetooth
{
    struct SGattClientProfile
    {
        uint16_t appId;
        std::function<void(esp_gattc_cb_event_t, esp_gatt_if_t, esp_ble_gattc_cb_param_t*)> gattClientCallback = nullptr;
        uint16_t gattcIf = ESP_GATT_IF_NONE;
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
