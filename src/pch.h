#pragma once

#define nameof(var) #var

#include <esp_check.h>
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "Config/Pinout.h"
#include "Config/Device.h"
