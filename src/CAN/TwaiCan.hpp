#pragma once

#include <cstdint>
#include "pch.h"
#include <driver/gpio.h>
#include <driver/twai.h>
#include <stdexcept>
#include "SCanMessage.h"
#include "ACan.h"

#if SOC_TWAI_SUPPORTED == 0
#error "Chip does not support TWAI."
#endif

namespace ReadieFur::OpenTCU::CAN
{
    class TwaiCan : public ACan
    {
        twai_general_config_t _generalConfig;
        twai_timing_config_t _timingConfig;
        twai_filter_config_t _filterConfig;
        twai_handle_t _driverHandle;

        TwaiCan(twai_general_config_t generalConfig, twai_timing_config_t timingConfig, twai_filter_config_t filterConfig) : ACan(),
            _generalConfig(generalConfig), _timingConfig(timingConfig), _filterConfig(filterConfig) {}

        int Install()
        {
            esp_err_t res = ESP_OK;
            if ((res = twai_driver_install_v2(&_generalConfig, &_timingConfig, &_filterConfig, &_driverHandle)) != ESP_OK)
            {
                LOGE(nameof(CAN::TwaiCan), "Failed to install TWAI driver: %#08x", res);
                return res;
            }
            if ((res = twai_start_v2(_driverHandle)) != ESP_OK)
            {
                LOGE(nameof(CAN::TwaiCan), "Failed to start TWAI driver: %#08x", res);
                return res;
            }
            if ((res = twai_reconfigure_alerts_v2(_driverHandle, TWAI_ALERT_RX_DATA, NULL)) != ESP_OK)
            {
                LOGE(nameof(CAN::TwaiCan), "Failed to configure TWAI driver: %#08x", res);
                return res;
            }
            return 0;
        }

    public:
        static TwaiCan* Initialize(twai_general_config_t generalConfig, twai_timing_config_t timingConfig, twai_filter_config_t filterConfig)
        {
            TwaiCan* instance = new TwaiCan(generalConfig, timingConfig, filterConfig);
            if (instance == nullptr || instance->Install() == 0)
                return instance;

            delete instance;
            return nullptr;
        }

        ~TwaiCan()
        {
            twai_stop_v2(_driverHandle);
            twai_driver_uninstall_v2(_driverHandle);
        }

        esp_err_t Send(SCanMessage message, TickType_t timeout)
        {
            twai_message_t twaiMessage = {
                .identifier = message.id,
                .data_length_code = message.length
            };
            twaiMessage.extd = message.isExtended;
            twaiMessage.rtr = message.isRemote;
            for (int i = 0; i < message.length; i++)
                twaiMessage.data[i] = message.data[i];

            #ifdef USE_CAN_DRIVER_LOCK
            if (xSemaphoreTake(_driverMutex, timeout) != pdTRUE)
            {
                LOGW(nameof(CAN::TwaiCan), "Timeout.");
                return ESP_ERR_TIMEOUT;
            }
            #endif

            esp_err_t res = twai_transmit_v2(_driverHandle, &twaiMessage, timeout);
            #ifdef USE_CAN_DRIVER_LOCK
            xSemaphoreGive(_driverMutex);
            #endif
            if (res != ESP_OK)
                LOGE(nameof(CAN::TwaiCan), "Failed to send message: %i", res);

            return res;
        }

        esp_err_t Receive(SCanMessage* message, TickType_t timeout)
        {
            //Use the read alerts function to wait for a message to be received (instead of locking on the twai_receive function).
            uint32_t alerts;
            if (esp_err_t err = twai_read_alerts_v2(_driverHandle, &alerts, timeout) != ESP_OK)
                return err;
            //We don't need to check the alert type because we have only subscribed to the RX_DATA alert.

            #ifdef USE_CAN_DRIVER_LOCK
            if (xSemaphoreTake(_driverMutex, timeout) != pdTRUE)
            {
                LOGW(nameof(CAN::TwaiCan), "Timeout.");
                return ESP_ERR_TIMEOUT;
            }
            #endif

            twai_message_t twaiMessage;
            esp_err_t err = twai_receive_v2(_driverHandle, &twaiMessage, timeout);
            #ifdef USE_CAN_DRIVER_LOCK
            //TODO: Handle potential failing of this release. If this fails the program will enter a catastrophic state.
            xSemaphoreGive(_driverMutex);
            #endif
            if (err != ESP_OK)
            {
                LOGE(nameof(CAN::TwaiCan), "Failed to receive message: %i", err);
                return err;
            }

            message->id = twaiMessage.identifier;
            message->length = twaiMessage.data_length_code;
            message->isExtended = twaiMessage.extd;
            message->isRemote = twaiMessage.rtr;
            for (int i = 0; i < twaiMessage.data_length_code; i++)
                message->data[i] = twaiMessage.data[i];

            return ESP_OK;
        }
        
        esp_err_t GetStatus(uint32_t* status, TickType_t timeout)
        {
            #ifdef USE_CAN_DRIVER_LOCK
            if (xSemaphoreTake(_driverMutex, timeout) != pdTRUE)
            {
                LOGW(nameof(CAN::TwaiCan), "Timeout.");
                return ESP_ERR_TIMEOUT;
            }
            #endif
            
            // esp_err_t res = twai_get_status_info_v2(_driverHandle, (twai_status_info_t*)status);
            esp_err_t res = twai_read_alerts_v2(_driverHandle, status, timeout);

            #ifdef USE_CAN_DRIVER_LOCK
            xSemaphoreGive(_driverMutex);
            #endif

            return res;
        }
    };
};
