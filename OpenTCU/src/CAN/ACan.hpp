#pragma once

//https://stackoverflow.com/questions/9756893/how-to-implement-interfaces-in-c

#include <cstdint>
#include <esp_err.h>
#include <freertos/portmacro.h>
#include "SCanMessage.hpp"

//TODO: Add concurrency safety.
class ACan
{
public:
    virtual esp_err_t Send(SCanMessage message, TickType_t timeout = 0) = 0;
    virtual esp_err_t Receive(SCanMessage* message, TickType_t timeout = 0) = 0;
};
