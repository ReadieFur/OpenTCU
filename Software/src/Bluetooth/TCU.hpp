#pragma once

#include <Service/AService.hpp>
#include "BLE.hpp"

//https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/bluedroid/ble/gatt_client/tutorial/Gatt_Client_Example_Walkthrough.md
namespace ReadieFur::OpenTCU::Bluetooth
{
    class TCU : public Service::AService
    {
    protected:
        void RunServiceImpl() override
        {
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

            ServiceCancellationToken.WaitForCancellation();
        }

    public:
        TCU()
        {
            ServiceEntrypointStackDepth += 1024;
            AddDependencyType<BLE>();
        }
    };
};
