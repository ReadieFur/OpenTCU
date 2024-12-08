#pragma once

#include <esp_gatt_defs.h>
#include <mutex>
#include <esp_err.h>
#include <esp_gatts_api.h>
#include <Logging.hpp>
#include <Helpers.h>
#include <memory.h>
#include <functional>
#include <map>
#include <string.h>
#include "SUUID.hpp"

namespace ReadieFur::OpenTCU::Bluetooth
{
    static const uint16_t GATT_PRIMARY_SERVICE_UUID = ESP_GATT_UUID_PRI_SERVICE;
    static const uint16_t GATT_CHARACTERISTIC_DECLARATION_UUID = ESP_GATT_UUID_CHAR_DECLARE;
    static const uint16_t GATT_CHARACTERISTIC_DESCRIPTION_UUID = ESP_GATT_UUID_CHAR_DESCRIPTION;
    static const uint8_t GATT_CHARACTERISTIC_PROP_READ = ESP_GATT_CHAR_PROP_BIT_READ;
    static const uint8_t GATT_CHARACTERISTIC_PROP_WRITE = ESP_GATT_CHAR_PROP_BIT_WRITE;
    static const uint8_t GATT_CHARACTERISTIC_PROP_READ_WRITE = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE;
    typedef std::function<esp_gatt_status_t(uint8_t* outValue, uint16_t* outLength)> TGattServerReadCallback; //Set the parameter names for IDE hints.

    //https://github.com/espressif/esp-idf/blob/master/examples/bluetooth/bluedroid/ble/gatt_server/tutorial/Gatt_Server_Example_Walkthrough.md
    class GattServerService
    {
    private:
        struct SAttributeInfo
        {
            esp_gatts_attr_db_t* attribute;
            TGattServerReadCallback readCallback;
        };

        std::mutex _mutex;
        bool _frozen = false;
        uint8_t _instanceId;
        uint16_t* _handleTable = nullptr;
        size_t _uuidsSize = 1;
        SUUID* _uuids = (SUUID*)malloc(1 * sizeof(SUUID));
        size_t _attributesSize = 1;
        esp_gatts_attr_db_t* _rawAttributes = (esp_gatts_attr_db_t*)malloc(1 * sizeof(esp_gatts_attr_db_t));
        std::map<uint16_t, SUUID> _handleMap;
        std::map<SUUID, SAttributeInfo> _attributes;

        GattServerService() {}

        esp_err_t AddAttribute(
            SUUID uuid,
            const char* description,
            uint16_t permissions,
            void* value,
            uint16_t length,
            uint16_t maxLength,
            bool autoResponse,
            TGattServerReadCallback readCallback
        )
        {
            _mutex.lock();
            if (_frozen)
            {
                _mutex.unlock();
                return ESP_ERR_INVALID_STATE;
            }

            auto uuids = (SUUID*)realloc(_uuids, (_uuidsSize + 1) * sizeof(SUUID));
            if (uuids == nullptr)
            {
                LOGE(nameof(Bluetooth::GattServerService), "Failed to allocate memory for UUIDs.");
                _mutex.unlock();
                return ESP_ERR_NO_MEM;
            }
            _uuids = uuids;
            _uuids[_uuidsSize] = uuid;

            auto attributes = (esp_gatts_attr_db_t*)realloc(_rawAttributes, (_attributesSize + (description != NULL ? 3 : 2)) * sizeof(esp_gatts_attr_db_t));
            if (attributes == nullptr)
            {
                LOGE(nameof(Bluetooth::GattServerService), "Failed to allocate memory for attributes.");
                _mutex.unlock();
                return ESP_ERR_NO_MEM;
            }
            _rawAttributes = attributes;

            //Decleration.
            const uint8_t* declerationValue;
            if ((permissions & ESP_GATT_PERM_READ && permissions & ESP_GATT_PERM_WRITE) || (permissions & ESP_GATT_PERM_READ_ENCRYPTED && permissions & ESP_GATT_PERM_WRITE_ENCRYPTED))
                declerationValue = &GATT_CHARACTERISTIC_PROP_READ_WRITE;
            else if ((permissions & ESP_GATT_PERM_READ) || (permissions & ESP_GATT_PERM_READ_ENCRYPTED))
                declerationValue = &GATT_CHARACTERISTIC_PROP_READ;
            else if ((permissions & ESP_GATT_PERM_WRITE) || (permissions & ESP_GATT_PERM_WRITE_ENCRYPTED))
                declerationValue = &GATT_CHARACTERISTIC_PROP_WRITE;
            else
                declerationValue = 0;
            _rawAttributes[_attributesSize++] =
            {
                .attr_control = { .auto_rsp = ESP_GATT_AUTO_RSP },
                .att_desc =
                {
                    .uuid_length = ESP_UUID_LEN_16,
                    .uuid_p = (uint8_t*)&GATT_CHARACTERISTIC_DECLARATION_UUID,
                    .perm = ESP_GATT_PERM_READ,
                    .max_length = sizeof(uint8_t),
                    .length = sizeof(uint8_t),
                    .value = (uint8_t*)declerationValue
                }
            };

            //Description.
            if (description != NULL)
            {
                _rawAttributes[_attributesSize++] =
                {
                    .attr_control = { .auto_rsp = ESP_GATT_AUTO_RSP },
                    .att_desc =
                    {
                        .uuid_length = ESP_UUID_LEN_16,
                        .uuid_p = (uint8_t*)&GATT_CHARACTERISTIC_DESCRIPTION_UUID,
                        .perm = ESP_GATT_PERM_READ,
                        .max_length = (uint16_t)strlen(description),
                        .length = (uint16_t)strlen(description),
                        .value = (uint8_t*)description
                    }
                };
            }

            //Value.
            _rawAttributes[_attributesSize++] =
            {
                .attr_control = { .auto_rsp = (uint8_t)(autoResponse ? ESP_GATT_AUTO_RSP : ESP_GATT_RSP_BY_APP) },
                .att_desc =
                {
                    .uuid_length = (uint16_t)_uuids[_uuidsSize].Length(),
                    .uuid_p = (uint8_t*)_uuids[_uuidsSize].Data(),
                    .perm = permissions,
                    .max_length = maxLength,
                    .length = length,
                    .value = (uint8_t*)value
                }
            };

            _attributes[_uuids[_uuidsSize]] =
            {
                .attribute = &_rawAttributes[_attributesSize - 1],
                .readCallback = readCallback
            };

            _uuidsSize++;

            _mutex.unlock();
            return ESP_OK;
        }

    public:
        GattServerService(SUUID uuid, uint8_t instanceId) : _instanceId(instanceId)
        {
            //Keep UUIDs copied in memory so the calling method can go out of scope.
            _uuids[0] = uuid;
            _rawAttributes[0] =
            {
                .attr_control = { .auto_rsp = ESP_GATT_AUTO_RSP },
                .att_desc =
                {
                    .uuid_length = ESP_UUID_LEN_16,
                    .uuid_p = (uint8_t*)&GATT_PRIMARY_SERVICE_UUID,
                    .perm = ESP_GATT_PERM_READ,
                    .max_length = (uint16_t)_uuids[0].Length(),
                    .length = (uint16_t)_uuids[0].Length(),
                    .value = (uint8_t*)_uuids[0].Data() //TODO: Figure out why the values passed here do not reflect in the BLE client but the ones passed in the AddAttribute method do.
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

            if (_rawAttributes != nullptr)
            {
                free(_rawAttributes);
                _rawAttributes = nullptr;
            }

            if (_uuids != nullptr)
            {
                free(_uuids);
                _uuids = nullptr;
            }

            _mutex.unlock();
        }

        #pragma Region Auto responders
        esp_err_t AddAttribute(
            SUUID uuid,
            uint16_t permissions,
            void* value,
            uint16_t length,
            uint16_t maxLength
        )
        {
            return AddAttribute(uuid, nullptr, permissions, value, length, maxLength, true, nullptr);
        }

        esp_err_t AddAttribute(
            SUUID uuid,
            const char* description,
            uint16_t permissions,
            void* value,
            uint16_t length,
            uint16_t maxLength
        )
        {
            return AddAttribute(uuid, description, permissions, value, length, maxLength, true, nullptr);
        }
        #pragma endregion

        #pragma region Manual responders
        esp_err_t AddAttribute(
            SUUID uuid,
            uint16_t permissions,
            uint16_t maxLength,
            TGattServerReadCallback readCallback
        )
        {
            return AddAttribute(uuid, NULL, permissions, NULL, 0, maxLength, false, readCallback);
        }

        esp_err_t AddAttribute(
            SUUID uuid,
            const char* description,
            uint16_t permissions,
            uint16_t maxLength,
            TGattServerReadCallback readCallback
        )
        {
            return AddAttribute(uuid, description, permissions, NULL, 0, maxLength, false, readCallback);
        }
        #pragma endregion

        esp_err_t AddAttribute(esp_gatts_attr_db_t attribute)
        {
            _mutex.lock();
            if (_frozen)
            {
                _mutex.unlock();
                return ESP_ERR_INVALID_STATE;
            }

            auto attributes = (esp_gatts_attr_db_t*)realloc(_rawAttributes, (_attributesSize + 1) * sizeof(esp_gatts_attr_db_t));
            if (attributes == nullptr)
            {
                _mutex.unlock();
                return ESP_ERR_NO_MEM;
            }
            _rawAttributes = attributes;

            _rawAttributes[_attributesSize++] = attribute;

            _attributes[*(uint16_t*)attribute.att_desc.uuid_p] =
            {
                .attribute = &_rawAttributes[_attributesSize - 1]
            };

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

                esp_err_t err = esp_ble_gatts_create_attr_tab(_rawAttributes, gattsIf, _attributesSize, _instanceId);
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

                for (size_t i = 0; i < param->add_attr_tab.num_handle; i++)
                {

                    switch (_rawAttributes[i].att_desc.uuid_length)
                    {
                    case ESP_UUID_LEN_16:
                        _handleMap[param->add_attr_tab.handles[i]] = SUUID(*(uint16_t*)_rawAttributes[i].att_desc.uuid_p);
                        LOGV(nameof(Bluetooth::GattServerService), "Attribute handle: %d, UUID: %x", param->add_attr_tab.handles[i], *(uint16_t*)_rawAttributes[i].att_desc.uuid_p);
                        break;
                    case ESP_UUID_LEN_32:
                        _handleMap[param->add_attr_tab.handles[i]] = SUUID(*(uint32_t*)_rawAttributes[i].att_desc.uuid_p);
                        LOGV(nameof(Bluetooth::GattServerService), "Attribute handle: %d, UUID: %x", param->add_attr_tab.handles[i], *(uint32_t*)_rawAttributes[i].att_desc.uuid_p);
                        break;
                    case ESP_UUID_LEN_128:
                        _handleMap[param->add_attr_tab.handles[i]] = SUUID((uint8_t*)_rawAttributes[i].att_desc.uuid_p);
                        LOGV(nameof(Bluetooth::GattServerService), "Attribute handle: %d, UUID: (cba to log, too long)", param->add_attr_tab.handles[i]);
                        break;
                    default:
                        LOGV(nameof(Bluetooth::GattServerService), "Attribute handle: %d, UUID length: %d, No match!", param->add_attr_tab.handles[i], _rawAttributes[i].att_desc.uuid_length);
                        continue;
                    }

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
                {
                    esp_ble_gatts_send_response(gattsIf, param->read.conn_id, param->read.trans_id, ESP_GATT_NO_RESOURCES, nullptr);
                    break;
                }

                if (!_attributes.contains(_handleMap[param->read.handle]))
                {
                    LOGW(nameof(Bluetooth::GattServerService), "Attribute not found for handle: %d", param->read.handle);
                    esp_ble_gatts_send_response(gattsIf, param->read.conn_id, param->read.trans_id, ESP_GATT_NOT_FOUND, nullptr);
                    break;
                }

                SAttributeInfo attributeInfo = _attributes[_handleMap[param->read.handle]];

                if (attributeInfo.attribute->attr_control.auto_rsp == ESP_GATT_AUTO_RSP)
                {
                    //If auto response is set, the BLE stack will handle getting and setting the balue, so just return here.
                    break;
                }

                esp_gatt_rsp_t rsp;
                memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
                rsp.attr_value.handle = param->read.handle;

                esp_gatt_status_t status;
                if (attributeInfo.readCallback != nullptr)
                {
                    status = attributeInfo.readCallback(rsp.attr_value.value, &rsp.attr_value.len);
                }
                else
                {
                    //If the write callback is not set, return the value as is.
                    rsp.attr_value.len = attributeInfo.attribute->att_desc.length;
                    memcpy(rsp.attr_value.value, attributeInfo.attribute->att_desc.value, rsp.attr_value.len);
                    status = ESP_GATT_OK;
                }

                esp_ble_gatts_send_response(gattsIf, param->read.conn_id, param->read.trans_id, status, &rsp);
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
