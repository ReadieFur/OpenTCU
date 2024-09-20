#pragma once

#include <cstdint>
#include <esp_err.h>
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
public:
    TwaiCan(twai_general_config_t generalConfig, twai_timing_config_t timingConfig, twai_filter_config_t filterConfig) : ACan()
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(twai_driver_install(&generalConfig, &timingConfig, &filterConfig));
        ESP_ERROR_CHECK_WITHOUT_ABORT(twai_start());
        ESP_ERROR_CHECK_WITHOUT_ABORT(twai_reconfigure_alerts(TWAI_ALERT_RX_DATA, NULL));
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

        #ifdef USE_DRIVER_LOCK
        if (xSemaphoreTake(_driverMutex, timeout) != pdTRUE)
            return ESP_ERR_TIMEOUT;
        #endif

        esp_err_t res = twai_transmit(&twaiMessage, timeout);
        #ifdef USE_DRIVER_LOCK
        xSemaphoreGive(_driverMutex);
        #endif

        return res;
    }

    esp_err_t Receive(SCanMessage* message, TickType_t timeout)
    {
        //Use the read alerts function to wait for a message to be received (instead of locking on the twai_receive function).
        uint32_t alerts;
        if (esp_err_t err = twai_read_alerts(&alerts, timeout) != ESP_OK)
            return err;
        //We don't need to check the alert type because we have only subscribed to the RX_DATA alert.

        #ifdef USE_DRIVER_LOCK
        if (xSemaphoreTake(_driverMutex, timeout) != pdTRUE)
            return ESP_ERR_TIMEOUT;
        #endif

        twai_message_t twaiMessage;
        esp_err_t err = twai_receive(&twaiMessage, timeout);

        #ifdef USE_DRIVER_LOCK
        xSemaphoreGive(_driverMutex);
        #endif

        if (err != ESP_OK)
            return err;

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
        #ifdef USE_DRIVER_LOCK
        if (xSemaphoreTake(_driverMutex, timeout) != pdTRUE)
            return ESP_ERR_TIMEOUT;
        #endif
        
        // esp_err_t res = twai_get_status_info((twai_status_info_t*)status);
        esp_err_t res = twai_read_alerts(status, timeout);

        #ifdef USE_DRIVER_LOCK
        xSemaphoreGive(_driverMutex);
        #endif

        return res;
    }
};
