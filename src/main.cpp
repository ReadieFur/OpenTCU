#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include "StaticConfig.h"
#include "CAN/BusMaster.hpp"
#include "Power/PowerManager.hpp"
#include "Bluetooth/BluetoothMaster.hpp"

static_assert(ESP_NAME[0] != '\0', "ESP_NAME cannot be empty.");

void setup()
{
    //It is critical that the BusMaster is the first service to be started.
    BusMaster::Begin();
    PowerManager::Begin();
    BluetoothMaster::Begin();
}

void loop()
{
    //I don't need this loop method as this program is task based.
    vTaskDelete(NULL);
}
