#pragma once

#include <cstdint>
#include <esp_err.h>
#include <driver/gpio.h>
#include <driver/twai.h>
#include <stdexcept>
#include "SCanMessage.h"
#include "ACan.h"
#include "Logging.h"

class TwaiCan : public ACan
{
public:
    TwaiCan(twai_general_config_t generalConfig, twai_timing_config_t timingConfig, twai_filter_config_t filterConfig) : ACan()
    {
        ASSERT(twai_driver_install(&generalConfig, &timingConfig, &filterConfig) == ESP_OK);
        ASSERT(twai_start() == ESP_OK);
        ASSERT(twai_reconfigure_alerts(TWAI_ALERT_RX_DATA, NULL) == ESP_OK);
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

        LOG_TRACE("TWAI Write: %x, %d, %d, %d, %x", twaiMessage.identifier, twaiMessage.data_length_code, twaiMessage.extd, twaiMessage.rtr, twaiMessage.data[0]);

        #ifdef USE_DRIVER_LOCK
        if (xSemaphoreTake(_driverMutex, timeout) != pdTRUE)
        {
            LOG_TRACE("TWAI Write Timeout");
            return ESP_ERR_TIMEOUT;
        }
        #endif
        esp_err_t res = twai_transmit(&twaiMessage, timeout);
        #ifdef USE_DRIVER_LOCK
        xSemaphoreGive(_driverMutex);
        #endif

        if (res != ESP_OK)
        {
            LOG_ERROR("TWAI Write Error: %d", res);
        }
        else
        {
            LOG_TRACE("TWAI Write Done");
        }

        return res;
    }

    esp_err_t Receive(SCanMessage* message, TickType_t timeout)
    {
        //Use the read alerts function to wait for a message to be received (instead of locking on the twai_receive function).
        LOG_TRACE("TWAI Wait");

        uint32_t alerts;
        if (esp_err_t err = twai_read_alerts(&alerts, timeout) != ESP_OK)
        {
            LOG_TRACE("TWAI Wait Timeout: %d", err);
            return err;
        }
        //We don't need to check the alert type because we have only subscribed to the RX_DATA alert.
        // else if (!(alerts & TWAI_ALERT_RX_DATA))
        // {
        //     // TRACE("TWAI Wait Timeout: %d", alerts);
        //     return ESP_ERR_INVALID_RESPONSE;
        // }

        LOG_TRACE("TWAI Read");

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
        {
            LOG_ERROR("TWAI Read Error: %d", err);
            return err;
        }

        LOG_TRACE("TWAI Read Done: %x, %d, %d, %d, %x", twaiMessage.identifier, twaiMessage.data_length_code, twaiMessage.extd, twaiMessage.rtr, twaiMessage.data[0]);

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
        {
            LOG_TRACE("TWAI Status Timeout");
            return ESP_ERR_TIMEOUT;
        }
        #endif
        
        // esp_err_t res = twai_get_status_info((twai_status_info_t*)status);
        esp_err_t res = twai_read_alerts(status, timeout);

        #ifdef USE_DRIVER_LOCK
        xSemaphoreGive(_driverMutex);
        #endif

        LOG_TRACE("TWAI Status: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d",
            *status & TWAI_ALERT_TX_IDLE,
            *status & TWAI_ALERT_TX_SUCCESS,
            *status & TWAI_ALERT_RX_DATA,
            *status & TWAI_ALERT_BELOW_ERR_WARN,
            *status & TWAI_ALERT_ERR_ACTIVE,
            *status & TWAI_ALERT_RECOVERY_IN_PROGRESS,
            *status & TWAI_ALERT_BUS_RECOVERED,
            *status & TWAI_ALERT_ARB_LOST,
            *status & TWAI_ALERT_ABOVE_ERR_WARN,
            *status & TWAI_ALERT_BUS_ERROR,
            *status & TWAI_ALERT_TX_FAILED,
            *status & TWAI_ALERT_RX_QUEUE_FULL,
            *status & TWAI_ALERT_ERR_PASS,
            *status & TWAI_ALERT_BUS_OFF,
            *status & TWAI_ALERT_RX_FIFO_OVERRUN,
            *status & TWAI_ALERT_TX_RETRIED,
            *status & TWAI_ALERT_PERIPH_RESET,
            *status & TWAI_ALERT_ALL,
            *status & TWAI_ALERT_NONE,
            *status & TWAI_ALERT_AND_LOG
        );

        return res;
    }
};
