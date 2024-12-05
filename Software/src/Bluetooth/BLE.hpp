#pragma once

#include <Service/AService.hpp>
#include <nvs_flash.h>
#include <esp_bt.h>
#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>
#include <esp_gattc_api.h>
#include <esp_gatt_defs.h>
#include <esp_bt_main.h>
#include <esp_gatt_common_api.h>
#include <Logging.hpp>
#include <map>
#include <functional>
#include <freertos/semphr.h>
#include "Environment.h"

#define MS_TO_BLE_ADVERTISING_INTERVAL(ms) (ms / 0.625) //Advertising interval (in 0.625 ms units).

namespace ReadieFur::OpenTCU::Bluetooth
{
    class BLE : public Service::AService
    {
    private:
        typedef std::function<void(esp_gatt_if_t, esp_ble_gatts_cb_param_t*)> TServerCallback;
        typedef std::function<void(esp_gatt_if_t, esp_ble_gattc_cb_param_t*)> TClientCallback;

        static SemaphoreHandle_t _instanceMutex;
        static std::map<uint16_t, TServerCallback> _serverCallbacks;
        static std::map<uint16_t, TClientCallback> _clientCallbacks;

        static void GapCallback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param)
        {
            switch (event)
            {
                case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
                    LOGV(nameof(Bluetooth::BLE), "Advertisement data set complete");
                    break;
                case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
                    if (param->adv_start_cmpl.status == ESP_OK)
                        LOGV(nameof(Bluetooth::BLE), "Advertising started successfully");
                    else
                        ESP_LOGE("BLE", "Advertising start failed");
                    break;
                case ESP_GAP_BLE_SCAN_RESULT_EVT:
                    LOGV(nameof(Bluetooth::BLE), "Scan result event received");
                    // Handle scanning events if needed
                    break;
                // case ESP_GAP_BLE_CONNECT_EVT:
                //     LOGV(nameof(Bluetooth::BLE), "Device connected");
                //     // Handle BLE connection events
                //     break;
                // case ESP_GAP_BLE_DISCONNECT_EVT:
                //     LOGV(nameof(Bluetooth::BLE), "Device disconnected");
                //     // Handle BLE disconnection events
                //     break;
                default:
                    break;
            }
        }

        static void GattServerCallback(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param)
        {
            if (_serverCallbacks.contains(param->reg.app_id))
                _serverCallbacks[param->reg.app_id](gatts_if, param);
        }

        static void GattClientCallback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t* param)
        {
            if (_clientCallbacks.contains(param->reg.app_id))
                _clientCallbacks[param->reg.app_id](gattc_if, param);
        }

    protected:
        void RunServiceImpl() override
        {
            if (xSemaphoreTake(_instanceMutex, 0) == pdFALSE)
            {
                LOGW(nameof(Bluetooth::BLE), "BLE instance already running.");
                return;
            }

            esp_err_t err = nvs_flash_init();
            if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
            {
                nvs_flash_erase();
                err = nvs_flash_init();
            }
            if (err != ESP_OK)
                return;

            //Initialize BLE controller.
            esp_bt_controller_config_t btCfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
            if ((err = esp_bt_controller_init(&btCfg)) != ESP_OK)
            {
                LOGE(nameof(Bluetooth::BLE), "Initialize BLE controller failed: %x", err);
                return;
            }
            if ((err = esp_bt_controller_enable(ESP_BT_MODE_BLE)) != ESP_OK)
            {
                LOGE(nameof(Bluetooth::BLE), "Enable BLE controller failed: %x", err);
                return;
            }

            if ((err = esp_bluedroid_init()) != ESP_OK)
            {
                LOGE(nameof(Bluetooth::BLE), "Init bluetooth failed: %x", err);
                return;
            }
            if ((err = esp_bluedroid_enable()) != ESP_OK)
            {
                LOGE(nameof(Bluetooth::BLE), "Enable bluetooth failed: %x", err);
                return;
            }

            esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
            if (local_mtu_ret)
            {
                LOGE(nameof(Bluetooth::BLE), "Set local MTU failed: %x", local_mtu_ret);
                return;
            }

            //Callbacks.
            if ((err = esp_ble_gap_register_callback(GapCallback)) != ESP_OK)
            {
                LOGE(nameof(Bluetooth::BLE), "Gap register failed: %x", err);
                return;
            }
            if ((err = esp_ble_gatts_register_callback(GattServerCallback)) != ESP_OK)
            {
                LOGE(nameof(Bluetooth::BLE), "GATT server callback registration failed: %x", err);
                return;
            }
            if ((err = esp_ble_gattc_register_callback(GattClientCallback)) != ESP_OK)
            {
                LOGE(nameof(Bluetooth::BLE), "GATT client callback registration failed: %x", err);
                return;
            }

            //Broadcasting.
            if ((err = esp_ble_gap_set_device_name(DeviceName.c_str())) != ESP_OK)
            {
                LOGE(nameof(Bluetooth::BLE), "Set device name failed: %x", err);
                return;
            }
            uint16_t advertisingIntervalMin = MS_TO_BLE_ADVERTISING_INTERVAL(20);
            uint16_t advertisingIntervalMax = MS_TO_BLE_ADVERTISING_INTERVAL(40);
            esp_ble_adv_data_t advertiseData = {
                .set_scan_rsp = false,
                .include_name = true,
                .include_txpower = true,
                .min_interval = advertisingIntervalMin,
                .max_interval = advertisingIntervalMax,
            };
            if ((err = esp_ble_gap_config_adv_data(&advertiseData)) != ESP_OK)
            {
                LOGE(nameof(Bluetooth::BLE), "Config advertising data failed: %x", err);
                return;
            }
            esp_ble_adv_params_t advertiseParams = {
                .adv_int_min = advertisingIntervalMin,
                .adv_int_max = advertisingIntervalMax,
                .adv_type = ADV_TYPE_IND, // Connectable undirected advertisement
                .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
                .peer_addr = {0},
                .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
                .channel_map = ADV_CHNL_ALL,
                .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
            };
            if ((err = esp_ble_gap_start_advertising(&advertiseParams)) != ESP_OK)
            {
                LOGE(nameof(Bluetooth::BLE), "Start advertising failed: %x", err);
                return;
            }

            ServiceCancellationToken.WaitForCancellation();

            if ((err = esp_bluedroid_disable()) != ESP_OK)
                LOGE(nameof(Bluetooth::BLE), "Disable bluetooth failed: %x", err);

            if ((err = esp_bluedroid_deinit()) != ESP_OK)
                LOGE(nameof(Bluetooth::BLE), "Deinit bluetooth failed: %x", err);

            if ((err = esp_bt_controller_disable()) != ESP_OK)
                LOGE(nameof(Bluetooth::BLE), "Disable BLE controller failed: %x", err);

            if ((err = esp_bt_controller_deinit()) != ESP_OK)
                LOGE(nameof(Bluetooth::BLE), "Deinit BLE controller failed: %x", err);

            _serverCallbacks.clear();
            _clientCallbacks.clear();

            xSemaphoreGive(_instanceMutex);
        }

    public:
        BLE()
        {
            ServiceEntrypointStackDepth += 1024;
        }

        esp_err_t RegisterServerApp(uint16_t appId, TServerCallback callback)
        {
            if (_serverCallbacks.contains(appId))
                return ESP_ERR_INVALID_STATE;

            esp_err_t retVal = esp_ble_gatts_app_register(appId);
            if (retVal != ESP_OK)
                return retVal;

            _serverCallbacks[appId] = callback;
            return ESP_OK;
        }

        esp_err_t RegisterClientApp(uint16_t appId, TClientCallback callback)
        {
            if (_clientCallbacks.contains(appId))
                return ESP_ERR_INVALID_STATE;
                
            esp_err_t retVal = esp_ble_gattc_app_register(appId);
            if (retVal != ESP_OK)
                return retVal;

            _clientCallbacks[appId] = callback;
            return ESP_OK;
        }

        esp_err_t UnregisterServerApp(uint16_t appId)
        {
            if (!_serverCallbacks.contains(appId))
                return ESP_ERR_INVALID_STATE;

            esp_err_t retVal = esp_ble_gatts_app_unregister(appId);
            if (retVal != ESP_OK)
                return retVal;

            _serverCallbacks.erase(appId);
            return ESP_OK;
        }

        esp_err_t UnregisterClientApp(uint16_t appId)
        {
            if (!_clientCallbacks.contains(appId))
                return ESP_ERR_INVALID_STATE;

            esp_err_t retVal = esp_ble_gattc_app_unregister(appId);
            if (retVal != ESP_OK)
                return retVal;

            _clientCallbacks.erase(appId);
            return ESP_OK;
        }
    };
};

SemaphoreHandle_t ReadieFur::OpenTCU::Bluetooth::BLE::_instanceMutex = xSemaphoreCreateMutex();
std::map<uint16_t, ReadieFur::OpenTCU::Bluetooth::BLE::TServerCallback> ReadieFur::OpenTCU::Bluetooth::BLE::_serverCallbacks;
std::map<uint16_t, ReadieFur::OpenTCU::Bluetooth::BLE::TClientCallback> ReadieFur::OpenTCU::Bluetooth::BLE::_clientCallbacks;
