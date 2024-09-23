#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include "Common.h"
#include "Config/Device.h"
#include "CAN/BusMaster.hpp"
#include "Power/PowerManager.hpp"

static_assert(ESP_NAME[0] != '\0', "ESP_NAME cannot be empty.");

#define ABORT_ON_FAIL(a, format) do {                                                   \
        if (unlikely(!(a))) {                                                           \
            ESP_LOGE("main", "%s(%d): " format ": %i", __FUNCTION__, __LINE__, a);      \
            abort();                                                                    \
        }                                                                               \
    } while(0)

PowerManager* powerManager;
BusMaster* busMaster;

void setup()
{
    powerManager = new PowerManager();
    ABORT_ON_FAIL(powerManager->InstallService(), "Failed to install " nameof(PowerManager) " service");
    ABORT_ON_FAIL(powerManager->StartService(), "Failed to start " nameof(PowerManager) " service");
    powerManager->OnPowerStateChanged.Add([](EPowerState powerState)
    {
        if (powerState != EPowerState::BatteryCritical)
            return;
            
        //TODO: Prepare shutdown.
    });

    busMaster = new BusMaster();
    ABORT_ON_FAIL(busMaster->InstallService(), "Failed to install " nameof(BusMaster) " service");
    ABORT_ON_FAIL(powerManager->AddService(busMaster, { EPowerState::PluggedIn }), "Failed to start " nameof(BusMaster) " service");
}

void loop()
{
    //I don't need this loop method as this program is task based.
    vTaskDelete(NULL);
}
