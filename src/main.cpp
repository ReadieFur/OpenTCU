#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include "CAN/BusMaster.hpp"

void setup()
{
    //It is critical that this is the first service to be started.
    BusMaster::Begin();
}

void loop()
{
    //I don't need this loop method as this program is task based.
    vTaskDelete(NULL);
}
