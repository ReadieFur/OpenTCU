#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include "pch.h"
#include "Config/Device.h"
#include "CAN/BusMaster.hpp"
#include "Config/JsonFlash.hpp"
#include <esp_sleep.h>

#define ABORT_ON_FAIL(a, format) do {                                                   \
        if (unlikely(!(a))) {                                                           \
            ESP_LOGE("main", "%s(%d): " format ": %i", __FUNCTION__, __LINE__, a);      \
            abort();                                                                    \
        }                                                                               \
    } while(0)

using namespace ReadieFur::OpenTCU;

Config::JsonFlash* _config;
CAN::BusMaster* _busMaster;

void setup()
{
    _config = Config::JsonFlash::Open("config.json");
    ABORT_ON_FAIL(_config != nullptr, "Failed to open config " nameof(Config::JsonFlash) " service");

    _busMaster = new CAN::BusMaster(_config);
    ABORT_ON_FAIL(_busMaster->InstallService(), "Failed to install " nameof(CAN::BusMaster) " service");
}

void loop()
{
    //I don't need this loop method as this program is task based.
    vTaskDelete(NULL);
}

#ifndef ARDUINO
extern "C" void app_main()
{
    setup();
    while (eTaskGetState(NULL) != eTaskState::eDeleted)
    {
        loop();
        portYIELD();
    }
}
#endif
