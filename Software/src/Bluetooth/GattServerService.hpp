#pragma once

#include <esp_gatt_defs.h>
#include <any>
#include <mutex>
#include <esp_err.h>
#include <esp_gatts_api.h>
#include <Logging.hpp>
#include <Helpers.h>
#include <memory.h>

namespace ReadieFur::OpenTCU::Bluetooth
{
    static const uint16_t GATT_PRIMARY_SERVICE_UUID = ESP_GATT_UUID_PRI_SERVICE;
    static const uint16_t GATT_CHARACTERISTIC_DECLARATION_UUID = ESP_GATT_UUID_CHAR_DECLARE;

    class GattServerService
    {
    private:
        std::mutex _mutex;
        bool _frozen = false;
        uint16_t* _handleTable = nullptr;
        size_t _uuidsSize = 1;
        uint16_t* _uuids = (uint16_t*)malloc(1 * sizeof(uint16_t));
        size_t _attributesSize = 1;
        esp_gatts_attr_db_t* _attributes = (esp_gatts_attr_db_t*)malloc(1 * sizeof(esp_gatts_attr_db_t));
        uint8_t _instanceId;

        GattServerService() {}

    public:
        GattServerService(uint16_t uuid, uint8_t instanceId) : _instanceId(instanceId)
        {
            //Keep UUIDs copied in memory so the calling method can go out of scope.
            _uuids[0] = uuid;
            _attributes[0] =
            {
                .attr_control = { .auto_rsp = ESP_GATT_AUTO_RSP },
                .att_desc =
                {
                    .uuid_length = ESP_UUID_LEN_16,
                    // .uuid_p = (uint8_t*)GetAttributeType(EGattAttributeType::PrimaryService),
                    .uuid_p = (uint8_t*)&GATT_PRIMARY_SERVICE_UUID,
                    .perm = ESP_GATT_PERM_READ,
                    .max_length = sizeof(uint16_t),
                    .length = sizeof(_uuids[0]),
                    .value = (uint8_t*)&_uuids[0]
                }
            };
        }

        ~GattServerService()
        {
            _mutex.lock();

            if (_handleTable != nullptr)
            {
                delete[] _handleTable;
                _handleTable = nullptr;
            }

            if (_attributes != nullptr)
            {
                free(_attributes);
                _attributes = nullptr;
            }

            if (_uuids != nullptr)
            {
                free(_uuids);
                _uuids = nullptr;
            }

            _mutex.unlock();
        }

        esp_err_t AddAttribute(
            uint16_t uuid,
            uint16_t permissions,
            void* value,
            uint16_t length,
            uint16_t maxLength)
        {
            _mutex.lock();
            if (_frozen)
            {
                _mutex.unlock();
                return ESP_ERR_INVALID_STATE;
            }
            
            auto uuids = (uint16_t*)realloc(_attributes, (_uuidsSize + 1) * sizeof(uint16_t));
            if (uuids == nullptr)
            {
                LOGE(nameof(Bluetooth::GattServerService), "Failed to allocate memory for UUIDs.");
                _mutex.unlock();
                return ESP_ERR_NO_MEM;
            }
            _uuids = uuids;
            _uuids[_uuidsSize] = uuid;
            uint16_t* _uuidPtr = &_uuids[_uuidsSize++];

            auto attributes = (esp_gatts_attr_db_t*)realloc(_attributes, (_attributesSize + 2) * sizeof(esp_gatts_attr_db_t));
            if (attributes == nullptr)
            {
                LOGE(nameof(Bluetooth::GattServerService), "Failed to allocate memory for attributes.");
                _mutex.unlock();
                return ESP_ERR_NO_MEM;
            }
            _attributes = attributes;

            //Decleration.
            _attributes[_attributesSize++] =
            {
                .attr_control = { .auto_rsp = ESP_GATT_AUTO_RSP },
                .att_desc =
                {
                    .uuid_length = ESP_UUID_LEN_16,
                    .uuid_p = (uint8_t*)&GATT_CHARACTERISTIC_DECLARATION_UUID,
                    .perm = ESP_GATT_PERM_READ,
                    .max_length = sizeof(uint16_t),
                    .length = sizeof(uuid),
                    .value = (uint8_t*)_uuidPtr
                }
            };

            //Value.
            _attributes[_attributesSize++] =
            {
                .attr_control = { .auto_rsp = ESP_GATT_RSP_BY_APP },
                .att_desc =
                {
                    .uuid_length = ESP_UUID_LEN_16,
                    .uuid_p = (uint8_t*)_uuidPtr,
                    .perm = permissions,
                    .max_length = maxLength,
                    .length = length,
                    .value = (uint8_t*)value
                }
            };

            _mutex.unlock();
            return ESP_OK;
        }

        esp_err_t AddAttribute(esp_gatts_attr_db_t attribute)
        {
            _mutex.lock();
            if (_frozen)
            {
                _mutex.unlock();
                return ESP_ERR_INVALID_STATE;
            }

            auto attributes = (esp_gatts_attr_db_t*)realloc(_attributes, (_attributesSize + 1) * sizeof(esp_gatts_attr_db_t));
            if (attributes == nullptr)
            {
                _mutex.unlock();
                return ESP_ERR_NO_MEM;
            }
            _attributes = attributes;

            _attributes[_attributesSize++] = attribute;

            _mutex.unlock();
            return ESP_OK;
        }

        void ProcessServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gattsIf, esp_ble_gatts_cb_param_t* param)
        {
            // LOGV(nameof(Bluetooth::GattServerService), "GATT_SVC_EVT: %d", event);

            _mutex.lock();

            switch (event)
            {
            case ESP_GATTS_REG_EVT:
            {
                if (_frozen)
                    break;

                //Freeze the object and register it.
                _handleTable = new uint16_t[_attributesSize];
                if (_handleTable == nullptr)
                {
                    _mutex.unlock();
                    break;
                }
                _frozen = true;

                esp_err_t err = esp_ble_gatts_create_attr_tab(_attributes, gattsIf, _attributesSize, _instanceId);
                if (err != ESP_OK)
                {
                    LOGE(nameof(Bluetooth::GattServerService), "Register attribute table failed: %s", esp_err_to_name(err));
                    break;
                }
                else
                {
                    LOGV(nameof(Bluetooth::GattServerService), "Registered %d attributes.", _attributesSize);
                }

                _frozen = true;
                break;
            }
            case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            {
                if (!_frozen)
                    break;

                if (param->add_attr_tab.svc_inst_id != _instanceId)
                    break;

                // LOGD(nameof(Bluetooth::GattServerServiceDatabase), "Attribute number handle: %i", param->add_attr_tab.num_handle);
                if (param->create.status != ESP_GATT_OK)
                {
                    LOGE(nameof(Bluetooth::GattServerService), "Create attribute table failed: %x", param->create.status);
                    break;
                }

                if (param->add_attr_tab.num_handle != _attributesSize)
                {
                    LOGE(nameof(Bluetooth::GattServerService), "Create attribute table abnormally, num_handle (%d) doesn't equal to _attributes.size(%d)", param->add_attr_tab.num_handle, _attributesSize);
                    break;
                }

                memcpy(_handleTable, param->add_attr_tab.handles, _attributesSize);
                esp_err_t err = esp_ble_gatts_start_service(_handleTable[0]);
                if (err != ESP_OK)
                    LOGE(nameof(Bluetooth::GattServerService), "Start service failed: %s", esp_err_to_name(err));
                else
                    LOGV(nameof(Bluetooth::GattServerService), "Started service: %ld", _handleTable[0]);
                break;
            }
            case ESP_GATTS_READ_EVT:
            {
                if (!_frozen)
                    break;

                //Get the attribute value.
                uint16_t length;
                const uint8_t* value;
                esp_gatt_status_t status = esp_ble_gatts_get_attr_value(param->read.handle, &length, &value);
                if (status != ESP_GATT_OK)
                {
                    LOGE(nameof(Bluetooth::GattServerService), "Read attribute failed: %s", esp_err_to_name(status));
                    break;
                }
                else
                {
                    LOGV(nameof(Bluetooth::GattServerService), "Read attribute: %d", *(uint16_t*)value);
                }
                break;
            }
            case ESP_GATTS_WRITE_EVT:
            {
                if (!_frozen)
                    break;

                //Set the attribute value.
                esp_err_t status = esp_ble_gatts_set_attr_value(param->write.handle, param->write.len, param->write.value);
                if (status != ESP_GATT_OK)
                {
                    LOGE(nameof(Bluetooth::GattServerService), "Write attribute failed: %s", esp_err_to_name(status));
                    break;
                }
                else
                {
                    LOGV(nameof(Bluetooth::GattServerService), "Wrote attribute: %d", *(uint16_t*)param->write.value);
                }
                break;
            }
            default:
                break;
            }

            _mutex.unlock();
        }
    };
};
