#pragma once

#include <cstdint>
#include <esp_err.h>
#include <driver/gpio.h>
#include <driver/twai.h>
#include <stdexcept>
#include "SCanMessage.h"
#include "ACan.h"
#include "Helpers.h"

class TwaiCan : public ACan
{
private:
    static bool initialized;

public:
    TwaiCan(twai_general_config_t generalConfig, twai_timing_config_t timingConfig, twai_filter_config_t filterConfig) : ACan()
    {
        #ifdef CONFIG_COMPILER_CXX_EXCEPTIONS
        if (initialized)
            throw std::runtime_error("Singleton class TwaiCan can only be initialized once.");
        initialized = true;

        if (esp_err_t err = twai_driver_install(&generalConfig, &timingConfig, &filterConfig) != ESP_OK)
            throw std::runtime_error("Failed to install TWAI driver: " + std::to_string(err));

        if (esp_err_t err = twai_start() != ESP_OK)
            throw std::runtime_error("Failed to start TWAI driver: " + std::to_string(err));

        if (esp_err_t err = twai_reconfigure_alerts(TWAI_ALERT_RX_DATA, NULL) != ESP_OK)
            throw std::runtime_error("Failed to reconfigure TWAI alerts: " + std::to_string(err));
        #else
        ASSERT(initialized == false);
        initialized = true;
        ASSERT(twai_driver_install(&generalConfig, &timingConfig, &filterConfig) == ESP_OK);
        ASSERT(twai_start() == ESP_OK);
        ASSERT(twai_reconfigure_alerts(TWAI_ALERT_RX_DATA, NULL) == ESP_OK);
        #endif
    }

    ~TwaiCan()
    {
        if (!initialized)
            return;

        twai_stop();
        twai_driver_uninstall();
        initialized = false;
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

        // TRACE("TWAI Write: %x, %d, %d, %d, %x", twaiMessage.identifier, twaiMessage.data_length_code, twaiMessage.extd, twaiMessage.rtr, twaiMessage.data[0]);
        #ifdef USE_DRIVER_LOCK
        if (xSemaphoreTake(driverMutex, timeout) != pdTRUE)
        {
            TRACE("TWAI Write Timeout");
            return ESP_ERR_TIMEOUT;
        }
        #endif
        esp_err_t res = twai_transmit(&twaiMessage, timeout);
        // TRACE("TWAI Write Done");
        #ifdef USE_DRIVER_LOCK
        xSemaphoreGive(driverMutex);
        #endif

        return res;
    }

    esp_err_t Receive(SCanMessage* message, TickType_t timeout)
    {
        #if defined(DEBUG) && 0
        //Get status.
        twai_status_info_t status;
        if (esp_err_t err = twai_get_status_info(&status) != ESP_OK)
        {
            TRACE("Failed to get TWAI status: %d", err);
        }
        else
        {
            TRACE("TWAI Status: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d", status.state, status.msgs_to_tx, status.msgs_to_rx, status.tx_error_counter, status.rx_error_counter, status.tx_failed_count, status.rx_missed_count, status.rx_overrun_count, status.arb_lost_count, status.bus_error_count);
        }
        #endif

        //Use the read alerts function to wait for a message to be received (instead of locking on the twai_receive function).
        uint32_t alerts;
        // TRACE("TWAI Wait");
        if (esp_err_t err = twai_read_alerts(&alerts, timeout) != ESP_OK)
        {
            // TRACE("TWAI Wait Timeout: %d", err);
            return err;
        }
        //We don't need to check the alert type because we have only subscribed to the RX_DATA alert.
        // else if (!(alerts & TWAI_ALERT_RX_DATA))
        // {
        //     // TRACE("TWAI Wait Timeout: %d", alerts);
        //     return ESP_ERR_INVALID_RESPONSE;
        // }
        // TRACE("TWAI Wait Done");

        twai_message_t twaiMessage;
        // TRACE("TWAI Read");
        #ifdef USE_DRIVER_LOCK
        if (xSemaphoreTake(driverMutex, timeout) != pdTRUE)
            return ESP_ERR_TIMEOUT;
        #endif
        esp_err_t err = twai_receive(&twaiMessage, timeout);
        // TRACE("TWAI Read Done: %x, %d, %d, %d, %x", twaiMessage.identifier, twaiMessage.data_length_code, twaiMessage.extd, twaiMessage.rtr, twaiMessage.data[0]);
        #ifdef USE_DRIVER_LOCK
        xSemaphoreGive(driverMutex);
        if (err != ESP_OK)
        {
            TRACE("TWAI Read Error: %d", err);
            return err;
        }
        #endif
        // TRACE("TWAI Read Success");

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
        // TRACE("TWAI Status");
        #ifdef USE_DRIVER_LOCK
        if (xSemaphoreTake(driverMutex, timeout) != pdTRUE)
        {
            // TRACE("TWAI Status Timeout");
            return ESP_ERR_TIMEOUT;
        }
        #endif
        // esp_err_t res = twai_get_status_info((twai_status_info_t*)status);
        esp_err_t res = twai_read_alerts(status, timeout);
        // TRACE("TWAI Status Done");
        #ifdef USE_DRIVER_LOCK
        xSemaphoreGive(driverMutex);
        #endif

        #if defined(DEBUG) && 0
        TRACE("TWAI Status: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d",
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
        #endif

        return res;
    }
};

bool TwaiCan::initialized = false;
