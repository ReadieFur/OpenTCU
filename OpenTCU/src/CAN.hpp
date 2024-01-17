#pragma once

// #define USE_SERIAL_CAN

#include <driver/gpio.h>
#include "Logger.hpp"
#ifdef USE_SERIAL_CAN
#else
#include <driver/twai.h>
#endif

class CAN
{
private:
    bool _initialized = false;

public:
    CAN(twai_general_config_t generalConfig, twai_timing_config_t timingConfig, twai_filter_config_t filterConfig)
    {
        if (esp_err_t err = twai_driver_install(&generalConfig, &timingConfig, &filterConfig) != ESP_OK)
        {
            Logger::Log("twai_driver_install failed: %#x\n", err);
            return;
        }

        _initialized = true;
    };

    ~CAN()
    {
        if (!_initialized)
            return;
        twai_driver_uninstall();
    }

    esp_err_t GetStatus(twai_status_info_t* statusInfo)
    {
        return twai_get_status_info(statusInfo);
    }

    esp_err_t Send(twai_message_t message, TickType_t ticksToWait = portMAX_DELAY)
    {
        return twai_transmit(&message, ticksToWait);
    }

    esp_err_t Read(twai_message_t* message, TickType_t ticksToWait = portMAX_DELAY)
    {
        return twai_receive(message, ticksToWait);
    }
};
