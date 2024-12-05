#pragma once

//https://stackoverflow.com/questions/9756893/how-to-implement-interfaces-in-c

#include <esp_err.h>
#include <freertos/portmacro.h>
#include <freertos/semphr.h>
#include "SCanMessage.h"

#define USE_CAN_DRIVER_LOCK

namespace ReadieFur::OpenTCU::CAN
{
    class ACan
    {
    protected:
        #ifdef USE_CAN_DRIVER_LOCK
        volatile SemaphoreHandle_t _driverMutex = xSemaphoreCreateMutex();
        #endif
    public:
        virtual ~ACan() = default;
        virtual esp_err_t Send(SCanMessage message, TickType_t timeout = 0) = 0;
        virtual esp_err_t Receive(SCanMessage* message, TickType_t timeout = 0) = 0;
        virtual esp_err_t GetStatus(uint32_t* status, TickType_t timeout = 0) = 0;
    };
};
