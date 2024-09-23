#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include "StaticConfig.h"
#include "CAN/BusMaster.hpp"

static_assert(ESP_NAME[0] != '\0', "ESP_NAME cannot be empty.");

BusMaster* busMaster;

void setup()
{
    busMaster = new BusMaster();
    busMaster->InstallService();
}

void loop()
{
    //I don't need this loop method as this program is task based.
    vTaskDelete(NULL);
}
