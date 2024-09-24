#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include "Common.h"
#include "Config/Device.h"
#include "CAN/BusMaster.hpp"
#include "Power/PowerManager.hpp"
#include "Bluetooth/BluetoothMaster.hpp"

#define ABORT_ON_FAIL(a, format) do {                                                   \
        if (unlikely(!(a))) {                                                           \
            ESP_LOGE("main", "%s(%d): " format ": %i", __FUNCTION__, __LINE__, a);      \
            abort();                                                                    \
        }                                                                               \
    } while(0)

using namespace ReadieFur::OpenTCU;

Power::PowerManager* powerManager;
CAN::BusMaster* busMaster;
Bluetooth::BluetoothMaster* bluetoothMaster;

void setup()
{
    powerManager = new Power::PowerManager();
    ABORT_ON_FAIL(powerManager->InstallService(), "Failed to install " nameof(PowerManager) " service");
    ABORT_ON_FAIL(powerManager->StartService(), "Failed to start " nameof(PowerManager) " service");
    powerManager->OnPowerStateChanged.Add([](Power::EPowerState powerState)
    {
        if (powerState != Power::EPowerState::BatteryCritical)
            return;
            
        //TODO: Prepare shutdown.
    });

    busMaster = new CAN::BusMaster();
    ABORT_ON_FAIL(busMaster->InstallService(), "Failed to install " nameof(BusMaster) " service");
    ABORT_ON_FAIL(powerManager->AddService(busMaster, { Power::EPowerState::PluggedIn }), "Failed to start " nameof(BusMaster) " service");

    bluetoothMaster = new Bluetooth::BluetoothMaster();
    bluetoothMaster->InstallService();
    powerManager->AddService(bluetoothMaster, { Power::EPowerState::PluggedIn });
}

void loop()
{
    //I don't need this loop method as this program is task based.
    vTaskDelete(NULL);
}
