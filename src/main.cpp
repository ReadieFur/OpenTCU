#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include "CAN/BusMaster.hpp"

void setup()
{
    //It is critical that this is the first service to be started.
    BusMaster::Begin();
}

#ifdef ARDUINO
void loop()
{
    vTaskDelete(NULL);
}
#else
extern "C" void app_main()
{
    setup();
    //app_main IS allowed to return as per the ESP32 documentation (other FreeRTOS tasks will continue to run).
}
#endif
