#include <freertos/FreeRTOS.h> //Has to always be the first included header.
#ifdef DEBUG
#include "Debug.hpp"
#endif
#include "Helpers.hpp"
#include "BusMaster.hpp"

void setup()
{
    //Used to indicate that the program has started.
    Helpers::ConfigureLED();
    Helpers::SetLed(0, 0, 1);

    #ifdef DEBUG
    Debug::Init();
    #endif

    //Initialize the CAN bus.
    BusMaster::Init();

    //Signal that the program has setup.
    Helpers::SetLed(false);

    //app_main IS allowed to return according to the ESP32 documentation (other FreeRTOS tasks will continue to run).
}

#ifdef ARDUINO
void loop()
{
    vTaskDelete(NULL);
}
#endif

#ifndef ARDUINO
extern "C" void app_main()
{
    setup();
    // while (true)
    //     loop();
}
#endif
