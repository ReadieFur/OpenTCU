#pragma once

#include <Service/AService.hpp>
#include "BLE.hpp"
#include "SGattServerProfile.h"
#include <esp_err.h>

#define HRPS_HT_MEAS_MAX_LEN            (13)

#define HRPS_MANDATORY_MASK             (0x0F)
#define HRPS_BODY_SENSOR_LOC_MASK       (0x30)
#define HRPS_HR_CTNL_PT_MASK            (0xC0)

#define HEART_RATE_SVC_INST_ID                    0

/// Heart Rate Sensor Service
static const uint16_t heart_rate_svc = ESP_GATT_UUID_HEART_RATE_SVC;

#define CHAR_DECLARATION_SIZE   (sizeof(uint8_t))
static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t char_prop_notify = ESP_GATT_CHAR_PROP_BIT_NOTIFY;
static const uint8_t char_prop_read = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_read_write = ESP_GATT_CHAR_PROP_BIT_WRITE|ESP_GATT_CHAR_PROP_BIT_READ;

/// Heart Rate Sensor Service - Heart Rate Measurement Characteristic, notify
static const uint16_t heart_rate_meas_uuid = ESP_GATT_HEART_RATE_MEAS;
static const uint8_t heart_measurement_ccc[2] ={ 0x00, 0x00};


/// Heart Rate Sensor Service -Body Sensor Location characteristic, read
static const uint16_t body_sensor_location_uuid = ESP_GATT_BODY_SENSOR_LOCATION;
static const uint8_t body_sensor_loc_val[1] = {0x00};


/// Heart Rate Sensor Service - Heart Rate Control Point characteristic, write&read
static const uint16_t heart_rate_ctrl_point = ESP_GATT_HEART_RATE_CNTL_POINT;
static const uint8_t heart_ctrl_point[1] = {0x00};

enum
{
    HRS_IDX_SVC,

    HRS_IDX_HR_MEAS_CHAR,
    HRS_IDX_HR_MEAS_VAL,
    HRS_IDX_HR_MEAS_NTF_CFG,

    HRS_IDX_BOBY_SENSOR_LOC_CHAR,
    HRS_IDX_BOBY_SENSOR_LOC_VAL,

    HRS_IDX_HR_CTNL_PT_CHAR,
    HRS_IDX_HR_CTNL_PT_VAL,

    HRS_IDX_NB,
};

/// Full HRS Database Description - Used to add attributes into the database
static const esp_gatts_attr_db_t heart_rate_gatt_db[HRS_IDX_NB] =
{
    // Heart Rate Service Declaration
    [HRS_IDX_SVC]                    =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ,
      sizeof(uint16_t), sizeof(heart_rate_svc), (uint8_t *)&heart_rate_svc}},

    // Heart Rate Measurement Characteristic Declaration
    [HRS_IDX_HR_MEAS_CHAR]            =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE,CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_notify}},

    // Heart Rate Measurement Characteristic Value
    [HRS_IDX_HR_MEAS_VAL]             =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&heart_rate_meas_uuid, ESP_GATT_PERM_READ,
      HRPS_HT_MEAS_MAX_LEN,0, NULL}},

    // Heart Rate Measurement Characteristic - Client Characteristic Configuration Descriptor
    [HRS_IDX_HR_MEAS_NTF_CFG]        =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,
      sizeof(uint16_t),sizeof(heart_measurement_ccc), (uint8_t *)heart_measurement_ccc}},

    // Body Sensor Location Characteristic Declaration
    [HRS_IDX_BOBY_SENSOR_LOC_CHAR]  =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE,CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read}},

    // Body Sensor Location Characteristic Value
    [HRS_IDX_BOBY_SENSOR_LOC_VAL]   =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&body_sensor_location_uuid, ESP_GATT_PERM_READ_ENCRYPTED,
      sizeof(uint8_t), sizeof(body_sensor_loc_val), (uint8_t *)body_sensor_loc_val}},

    // Heart Rate Control Point Characteristic Declaration
    [HRS_IDX_HR_CTNL_PT_CHAR]          =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ,
      CHAR_DECLARATION_SIZE,CHAR_DECLARATION_SIZE, (uint8_t *)&char_prop_read_write}},

    // Heart Rate Control Point Characteristic Value
    [HRS_IDX_HR_CTNL_PT_VAL]             =
    {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&heart_rate_ctrl_point, ESP_GATT_PERM_WRITE_ENCRYPTED|ESP_GATT_PERM_READ_ENCRYPTED,
      sizeof(uint8_t), sizeof(heart_ctrl_point), (uint8_t *)heart_ctrl_point}},
};

static uint16_t heart_rate_handle_table[HRS_IDX_NB];

namespace ReadieFur::OpenTCU::Bluetooth
{
    class API : public Service::AService
    {
    private:
        void ServerAppCallback(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param)
        {
            switch (event)
            {
            case ESP_GATTS_REG_EVT:
            {
                esp_ble_gatts_create_attr_tab(heart_rate_gatt_db, gatts_if, HRS_IDX_NB, HEART_RATE_SVC_INST_ID);
                break;
            }
            case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            {
                LOGI(nameof(Bluetooth::API), "The number handle = %x",param->add_attr_tab.num_handle);
                if (param->create.status == ESP_GATT_OK)
                {
                    if(param->add_attr_tab.num_handle == HRS_IDX_NB)
                    {
                        memcpy(heart_rate_handle_table, param->add_attr_tab.handles,
                        sizeof(heart_rate_handle_table));
                        esp_ble_gatts_start_service(heart_rate_handle_table[HRS_IDX_SVC]);
                    }
                    else
                    {
                        LOGE(nameof(Bluetooth::API), "Create attribute table abnormally, num_handle (%d) doesn't equal to HRS_IDX_NB(%d)",
                            param->add_attr_tab.num_handle, HRS_IDX_NB);
                    }
                }
                else
                {
                    LOGE(nameof(Bluetooth::API), " Create attribute table failed, error code = %x", param->create.status);
                }
                break;
            }
            default:
                break;
            }
        }

    protected:
        void RunServiceImpl() override
        {
            esp_err_t err;

            BLE* bleService = GetService<BLE>();

            SGattServerProfile serverProfile =
            {
                .appId = 0x01,
                .gattServerCallback = [this](auto a, auto b, auto c){ ServerAppCallback(a, b, c); },
            };
            if ((err = bleService->RegisterServerApp(&serverProfile)) != ESP_OK)
            {
                LOGE(nameof(Bluetooth::API), "Failed to register server app: %s", esp_err_to_name(err));
                return;
            }

            //TODO: Register services and characteristics.

            LOGD(nameof(Bluetooth::API), "BLE API started.");
            ServiceCancellationToken.WaitForCancellation();

            bleService->UnregisterServerApp(serverProfile.appId);
        }

    public:
        API()
        {
            ServiceEntrypointStackDepth += 1024;
            AddDependencyType<BLE>();
        }
    };
};
