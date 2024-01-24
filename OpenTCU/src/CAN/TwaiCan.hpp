#pragma once

#include <cstdint>
#include <esp_err.h>
#include <driver/gpio.h>
#include <driver/twai.h>
#include <stdexcept>
#include "SCanMessage.hpp"
#include "ACan.hpp"
#include "Helpers.h"

class TwaiCan : public ACan
{
private:
    static bool initialized;

public:
    TwaiCan(twai_general_config_t generalConfig, twai_timing_config_t timingConfig, twai_filter_config_t filterConfig) : ACan()
    {
        if (initialized)
            throw std::runtime_error("Singleton class TwaiCan can only be initialized once.");
        initialized = true;

        if (esp_err_t err = twai_driver_install(&generalConfig, &timingConfig, &filterConfig) != ESP_OK)
            throw std::runtime_error("Failed to install TWAI driver: " + std::to_string(err));

        if (esp_err_t err = twai_start() != ESP_OK)
            throw std::runtime_error("Failed to start TWAI driver: " + std::to_string(err));

        if (esp_err_t err = twai_reconfigure_alerts(TWAI_ALERT_RX_DATA, NULL) != ESP_OK)
            throw std::runtime_error("Failed to reconfigure TWAI alerts: " + std::to_string(err));
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

        // TRACE("TWAI Write");
        if (xSemaphoreTake(driverMutex, timeout) != pdTRUE)
            return ESP_ERR_TIMEOUT;
        esp_err_t res = twai_transmit(&twaiMessage, timeout);
        // TRACE("TWAI Write Done");
        xSemaphoreGive(driverMutex);

        return res;
    }

    esp_err_t Receive(SCanMessage* message, TickType_t timeout)
    {
        // #if DEBUG
        // //Get status.
        // twai_status_info_t status;
        // if (esp_err_t err = twai_get_status_info(&status) != ESP_OK)
        // {
        //     TRACE("Failed to get TWAI status: %d", err);
        // }
        // else
        // {
        //     TRACE("TWAI Status: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d", status.state, status.msgs_to_tx, status.msgs_to_rx, status.tx_error_counter, status.rx_error_counter, status.tx_failed_count, status.rx_missed_count, status.rx_overrun_count, status.arb_lost_count, status.bus_error_count);
        // }
        // #endif

        //Use the read alerts function to wait for a message to be received (instead of locking on the twai_receive function).
        uint32_t alerts;
        // TRACE("TWAI Wait");
        if (esp_err_t err = twai_read_alerts(&alerts, timeout) != ESP_OK)
            return err;
        // TRACE("TWAI Wait Done");

        twai_message_t twaiMessage;
        // TRACE("TWAI Read");
        if (xSemaphoreTake(driverMutex, timeout) != pdTRUE)
            return ESP_ERR_TIMEOUT;
        //We shouldn't wait for a message to be received so set the timeout to be minimal.
        esp_err_t err = twai_receive(&twaiMessage, pdMS_TO_TICKS(1));
        // TRACE("TWAI Read Done");
        xSemaphoreGive(driverMutex);
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

    esp_err_t GetStatus(twai_status_info_t &status)
    {
        return twai_get_status_info(&status);
    }
};

bool TwaiCan::initialized = false;
