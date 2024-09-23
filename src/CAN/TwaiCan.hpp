#pragma once

#include <cstdint>
#include "Common.h"
#include <driver/gpio.h>
#include <driver/twai.h>
#include <stdexcept>
#include "SCanMessage.h"
#include "ACan.h"

#if SOC_TWAI_SUPPORTED == 0
#error "Chip does not support TWAI."
#endif

class TwaiCan : public ACan
{
    twai_general_config_t _generalConfig;
    twai_timing_config_t _timingConfig;
    twai_filter_config_t _filterConfig;

    TwaiCan(twai_general_config_t generalConfig, twai_timing_config_t timingConfig, twai_filter_config_t filterConfig) : ACan(),
    _generalConfig(generalConfig), _timingConfig(timingConfig), _filterConfig(filterConfig) {}

    int Install()
    {
        ESP_RETURN_ON_FALSE(twai_driver_install(&_generalConfig, &_timingConfig, &_filterConfig) == ESP_OK, 1, nameof(TwaiCan), "Failed to install TWAI driver: %i", 1);
        ESP_RETURN_ON_FALSE(twai_start() == ESP_OK, 2, nameof(TwaiCan), "Failed to start TWAI driver: %i", 2);
        ESP_RETURN_ON_FALSE(twai_reconfigure_alerts(TWAI_ALERT_RX_DATA, NULL) == ESP_OK, 3, nameof(TwaiCan), "Failed to configure TWAI driver: %i", 3);
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
        twai_stop();
        twai_driver_uninstall();
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
        ESP_RETURN_ON_FALSE(xSemaphoreTake(_driverMutex, timeout) == pdTRUE, ESP_ERR_TIMEOUT, nameof(TwaiCan) "_dbg", "Timeout.");
        #endif

        esp_err_t res = twai_transmit(&twaiMessage, timeout);
        #ifdef USE_CAN_DRIVER_LOCK
        xSemaphoreGive(_driverMutex);
        #endif
        ESP_RETURN_ON_FALSE(res == ESP_OK, res, nameof(TwaiCan) "_dbg", "Failed to send message: %i", res);

        return res;
    }

    esp_err_t Receive(SCanMessage* message, TickType_t timeout)
    {
        //Use the read alerts function to wait for a message to be received (instead of locking on the twai_receive function).
        uint32_t alerts;
        if (esp_err_t err = twai_read_alerts(&alerts, timeout) != ESP_OK)
            return err;
        //We don't need to check the alert type because we have only subscribed to the RX_DATA alert.

        #ifdef USE_CAN_DRIVER_LOCK
        ESP_RETURN_ON_FALSE(xSemaphoreTake(_driverMutex, timeout) == pdTRUE, ESP_ERR_TIMEOUT, nameof(TwaiCan) "_dbg", "Timeout.");
        #endif

        twai_message_t twaiMessage;
        esp_err_t err = twai_receive(&twaiMessage, timeout);
        #ifdef USE_CAN_DRIVER_LOCK
        //TODO: Handle potential failing of this release. If this fails the program will enter a catastrophic state.
        xSemaphoreGive(_driverMutex);
        #endif
        ESP_RETURN_ON_FALSE(err == ESP_OK, err, nameof(TwaiCan) "_dbg", "Failed to receive message: %i", err);

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
        ESP_RETURN_ON_FALSE(xSemaphoreTake(_driverMutex, timeout) == pdTRUE, ESP_ERR_TIMEOUT, nameof(TwaiCan) "_dbg", "Timeout.");
        #endif
        
        // esp_err_t res = twai_get_status_info((twai_status_info_t*)status);
        esp_err_t res = twai_read_alerts(status, timeout);

        #ifdef USE_CAN_DRIVER_LOCK
        xSemaphoreGive(_driverMutex);
        #endif

        return res;
    }
};
