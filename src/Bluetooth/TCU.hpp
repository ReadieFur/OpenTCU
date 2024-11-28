#pragma once

#include <Service/AService.hpp>
#include <nvs_flash.h>
#include <esp_bt.h>
#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_gatt_defs.h>
#include <esp_bt_main.h>
#include <esp_gatt_common_api.h>
#include <Logging.hpp>

//https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/bluedroid/ble/gatt_client/tutorial/Gatt_Client_Example_Walkthrough.md
namespace ReadieFur::OpenTCU::Bluetooth
{
    class TCU : public Service::AService
    {
    protected:
        void RunServiceImpl() override
        {
            esp_err_t err = nvs_flash_init();
            if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
            {
                nvs_flash_erase();
                err = nvs_flash_init();
            }
            if (err != ESP_OK)
                return;

            esp_bt_controller_config_t btCfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
            if ((err = esp_bt_controller_init(&btCfg)) != ESP_OK)
            {
                LOGE(nameof(Bluetooth::TCU), "Initialize BLE controller failed: %x", err);
                return;
            }

            if ((err = esp_bt_controller_enable(ESP_BT_MODE_BLE)) != ESP_OK)
            {
                LOGE(nameof(Bluetooth::TCU), "Enable BLE controller failed: %x", err);
                return;
            }

            if ((err = esp_bluedroid_init()) != ESP_OK)
            {
                LOGE(nameof(Bluetooth::TCU), "Init bluetooth failed: %x", err);
                return;
            }

            if ((err = esp_bluedroid_enable()) != ESP_OK)
            {
                LOGE(nameof(Bluetooth::TCU), "Enable bluetooth failed: %x", err);
                return;
            }

            // //Register the callback function to the gap module.
            // if ((err = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK)
            // {
            //     LOGE(nameof(Bluetooth::TCU), "Gap register failed: %x", err);
            //     return;
            // }

            // //Register the callback function to the gattc module.
            // if ((err = esp_ble_gattc_register_callback(esp_gattc_cb)) != ESP_OK){
            //     LOGE(nameof(Bluetooth::TCU), "Gattc register failed: %x", err);
            //     return;
            // }

            // if ((err = esp_ble_gattc_app_register(PROFILE_A_APP_ID)) != ESP_OK)
            // {
            //     LOGE(nameof(Bluetooth::TCU), "Gattc app register failed: %x", err);
            //     return;
            // }

            esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
            if (local_mtu_ret)
            {
                LOGE(nameof(Bluetooth::TCU), "Set local MTU failed: %x", local_mtu_ret);
                return;
            }

            ServiceCancellationToken.WaitForCancellation();

            // if ((err = esp_bluedroid_disable()) != ESP_OK)
            //     LOGE(nameof(Bluetooth::TCU), "Disable bluetooth failed: %x", err);

            // if ((err = esp_bluedroid_deinit()) != ESP_OK)
            //     LOGE(nameof(Bluetooth::TCU), "Deinit bluetooth failed: %x", err);

            // if ((err = esp_bt_controller_disable()) != ESP_OK)
            //     LOGE(nameof(Bluetooth::TCU), "Disable BLE controller failed: %x", err);

            // if ((err = esp_bt_controller_deinit()) != ESP_OK)
            //     LOGE(nameof(Bluetooth::TCU), "Deinit BLE controller failed: %x", err);
        }

    public:
        TCU()
        {
            ServiceEntrypointStackDepth += 1024;
        }
    };
};
