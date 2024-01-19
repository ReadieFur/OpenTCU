#pragma once

#include <cstdint>
#include <esp_err.h>
#include <driver/gpio.h>
#include <driver/twai.h>
#include "SCanMessage.hpp"
#include "ACan.hpp"
#include <stdexcept>

class TwaiCan : public ACan
{
private:
    static bool initialized;

public:
    TwaiCan(twai_general_config_t generalConfig, twai_timing_config_t timingConfig, twai_filter_config_t filterConfig)
    {
        if (initialized)
            throw std::runtime_error("Singleton class TwaiCan can only be initialized once.");
        initialized = true;

        if (esp_err_t err = twai_driver_install(&generalConfig, &timingConfig, &filterConfig) != ESP_OK)
            throw std::runtime_error("Failed to install TWAI driver: " + std::to_string(err));

        if (esp_err_t err = twai_start() != ESP_OK)
            throw std::runtime_error("Failed to start TWAI driver: " + std::to_string(err));
    }

    ~TwaiCan()
    {
        if (!initialized)
            return;

        twai_stop();
        twai_driver_uninstall();
        initialized = false;
    }

    esp_err_t Send(SCanMessage message, TickType_t timeout = 0)
    {
        twai_message_t twaiMessage = {
            .identifier = message.id,
            .data_length_code = message.length
        };
        twaiMessage.extd = message.isExtended;
        twaiMessage.rtr = message.isRemote;
        for (int i = 0; i < message.length; i++)
            twaiMessage.data[i] = message.data[i];

        if (esp_err_t err = twai_transmit(&twaiMessage, timeout) != ESP_OK)
            return err;
        return ESP_OK;
    }

    esp_err_t Receive(SCanMessage* message, TickType_t timeout = 0)
    {
        twai_message_t twaiMessage;
        if (esp_err_t err = twai_receive(&twaiMessage, timeout) != ESP_OK)
            return err;

        message->id = twaiMessage.identifier;
        message->length = twaiMessage.data_length_code;
        message->isExtended = twaiMessage.extd;
        message->isRemote = twaiMessage.rtr;
        for (int i = 0; i < twaiMessage.data_length_code; i++)
            message->data[i] = twaiMessage.data[i];

        return ESP_OK;
    }

    esp_err_t GetAlerts(uint32_t& alerts, TickType_t timeout = 0)
    {
        return twai_read_alerts(&alerts, timeout);
    }

    esp_err_t GetStatus(twai_status_info_t &status)
    {
        return twai_get_status_info(&status);
    }
};

bool TwaiCan::initialized = false;
