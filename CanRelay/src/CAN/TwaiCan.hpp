#pragma once

#include <cstdint>
#include <esp_err.h>
#include <driver/gpio.h>
#include <driver/twai.h>
#include <stdexcept>
#include "SCanMessage.h"
#include "ACan.h"
#include <esp_check.h>
#include <esp_log.h>

class TwaiCan : public ACan
{
private:
    twai_handle_t _handle;

public:
    TwaiCan(twai_general_config_t generalConfig, twai_timing_config_t timingConfig, twai_filter_config_t filterConfig) : ACan()
    {
        ESP_ERROR_CHECK(twai_driver_install_v2(&generalConfig, &timingConfig, &filterConfig, &_handle));
        ESP_ERROR_CHECK(twai_start_v2(_handle));
        ESP_ERROR_CHECK(twai_reconfigure_alerts_v2(_handle, TWAI_ALERT_RX_DATA, NULL));
    }

    ~TwaiCan()
    {
        twai_stop_v2(_handle);
        twai_driver_uninstall_v2(_handle);
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

        // ESP_LOGV("twai", "TWAI Write: %x, %d, %d, %d, %x", twaiMessage.identifier, twaiMessage.data_length_code, twaiMessage.extd, twaiMessage.rtr, twaiMessage.data[0]);

        #ifdef USE_DRIVER_LOCK
        if (xSemaphoreTake(_driverMutex, timeout) != pdTRUE)
        {
            //ESP_LOGV("twai", "TWAI Write Timeout");
            return ESP_ERR_TIMEOUT;
        }
        #endif
        esp_err_t res = twai_transmit_v2(_handle, &twaiMessage, timeout);
        #ifdef USE_DRIVER_LOCK
        xSemaphoreGive(_driverMutex);
        #endif

        if (res != ESP_OK)
        {
            ESP_LOGE("twai", "TWAI Write Error: %s", esp_err_to_name(res));
        }
        // else
        // {
        //     //ESP_LOGV("twai", "TWAI Write Done");
        // }

        return res;
    }

    esp_err_t Receive(SCanMessage* message, TickType_t timeout)
    {
        //Use the read alerts function to wait for a message to be received (instead of locking on the twai_receive function).
        //ESP_LOGV("twai", "TWAI Wait");

        uint32_t alerts;
        if (esp_err_t err = twai_read_alerts_v2(_handle, &alerts, timeout) != ESP_OK)
        {
            //ESP_LOGV("twai", "TWAI Wait Timeout: %d", err);
            return err;
        }
        //We don't need to check the alert type because we have only subscribed to the RX_DATA alert.
        // else if (!(alerts & TWAI_ALERT_RX_DATA))
        // {
        //     // TRACE("TWAI Wait Timeout: %d", alerts);
        //     return ESP_ERR_INVALID_RESPONSE;
        // }

        //ESP_LOGV("twai", "TWAI Read");

        #ifdef USE_DRIVER_LOCK
        if (xSemaphoreTake(_driverMutex, timeout) != pdTRUE)
            return ESP_ERR_TIMEOUT;
        #endif

        twai_message_t twaiMessage;
        esp_err_t err = twai_receive_v2(_handle, &twaiMessage, timeout);

        #ifdef USE_DRIVER_LOCK
        xSemaphoreGive(_driverMutex);
        #endif

        if (err != ESP_OK)
        {
            ESP_LOGE("twai", "TWAI Read Error: %s", esp_err_to_name(err));
            return err;
        }

        // LOG_TRACE("TWAI Read Done: %x, %d, %d, %d, %x", twaiMessage.identifier, twaiMessage.data_length_code, twaiMessage.extd, twaiMessage.rtr, twaiMessage.data[0]);

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
            //ESP_LOGV("twai", "TWAI Status Timeout");
            return ESP_ERR_TIMEOUT;
        }
        #endif
        
        // esp_err_t res = twai_get_status_info((twai_status_info_t*)status);
        esp_err_t res = twai_read_alerts_v2(_handle, status, timeout);

        #ifdef USE_DRIVER_LOCK
        xSemaphoreGive(_driverMutex);
        #endif

        // LOG_TRACE("TWAI Status: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d",
        //     *status & TWAI_ALERT_TX_IDLE,
        //     *status & TWAI_ALERT_TX_SUCCESS,
        //     *status & TWAI_ALERT_RX_DATA,
        //     *status & TWAI_ALERT_BELOW_ERR_WARN,
        //     *status & TWAI_ALERT_ERR_ACTIVE,
        //     *status & TWAI_ALERT_RECOVERY_IN_PROGRESS,
        //     *status & TWAI_ALERT_BUS_RECOVERED,
        //     *status & TWAI_ALERT_ARB_LOST,
        //     *status & TWAI_ALERT_ABOVE_ERR_WARN,
        //     *status & TWAI_ALERT_BUS_ERROR,
        //     *status & TWAI_ALERT_TX_FAILED,
        //     *status & TWAI_ALERT_RX_QUEUE_FULL,
        //     *status & TWAI_ALERT_ERR_PASS,
        //     *status & TWAI_ALERT_BUS_OFF,
        //     *status & TWAI_ALERT_RX_FIFO_OVERRUN,
        //     *status & TWAI_ALERT_TX_RETRIED,
        //     *status & TWAI_ALERT_PERIPH_RESET,
        //     *status & TWAI_ALERT_ALL,
        //     *status & TWAI_ALERT_NONE,
        //     *status & TWAI_ALERT_AND_LOG
        // );

        return res;
    }
};
