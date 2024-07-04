#ifdef _DEBUG
#include "Debug.hpp"
#endif
#include "Helpers.hpp" //Include this first so that it takes priority over the ESP-IDF logging macros.
#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include "BusMaster.hpp"

void setup()
{
    //Used to indicate that the program has started.
    Helpers::ConfigureLED();
    Helpers::SetLed(0, 0, 1);

    #ifdef _DEBUG
    Debug::Init();
    #endif

    //Initialize the CAN bus.
    BusMaster::Init();
    BusMaster::Start();

    //Signal that the program has setup.
    Helpers::SetLed(0, 1, 0);
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
