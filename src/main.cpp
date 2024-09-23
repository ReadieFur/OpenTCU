#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include "Config/Device.h"
#include "CAN/BusMaster.hpp"
#include "Power/PowerManager.hpp"

static_assert(ESP_NAME[0] != '\0', "ESP_NAME cannot be empty.");

PowerManager* powerManager;
BusMaster* busMaster;

void setup()
{
    powerManager = new PowerManager();
    powerManager->InstallService();
    powerManager->StartService();

    busMaster = new BusMaster();
    busMaster->InstallService();
    powerManager->AddService(busMaster, EPowerState::PluggedIn);
}

void loop()
{
    //I don't need this loop method as this program is task based.
    vTaskDelete(NULL);
}
