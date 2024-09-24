#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include "Common.h"
#include "Config/Device.h"
#include "CAN/BusMaster.hpp"
#include "Power/PowerManager.hpp"
#include "Bluetooth/BluetoothMaster.hpp"
#include "GPS/GPSService.hpp"
#include "GSM/GSMService.hpp"

#define ABORT_ON_FAIL(a, format) do {                                                   \
        if (unlikely(!(a))) {                                                           \
            ESP_LOGE("main", "%s(%d): " format ": %i", __FUNCTION__, __LINE__, a);      \
            abort();                                                                    \
        }                                                                               \
    } while(0)

using namespace ReadieFur::OpenTCU;

Power::PowerManager* _powerManager;
CAN::BusMaster* _busMaster;
Bluetooth::BluetoothMaster* _bluetoothMaster;
GPS::GPSService* _gpsService;
GSM::GSMService* _gsmService;

void setup()
{
    #pragma region Critical services.
    _powerManager = new Power::PowerManager();
    ABORT_ON_FAIL(_powerManager->InstallService(), "Failed to install " nameof(PowerManager) " service");
    ABORT_ON_FAIL(_powerManager->StartService(), "Failed to start " nameof(PowerManager) " service");
    _powerManager->OnPowerStateChanged.Add([](Power::EPowerState powerState)
    {
        if (powerState != Power::EPowerState::BatteryCritical)
            return;
            
        //TODO: Prepare shutdown.
    });

    _busMaster = new CAN::BusMaster();
    ABORT_ON_FAIL(_busMaster->InstallService(), "Failed to install " nameof(BusMaster) " service");
    ABORT_ON_FAIL(_powerManager->AddService(_busMaster, { Power::EPowerState::PluggedIn }), "Failed to start " nameof(BusMaster) " service");
    #pragma endregion

    #pragma region Non-critical services.
    //TODO: Log these if they fail and retry, but don't abort.
    _bluetoothMaster = new Bluetooth::BluetoothMaster();
    _bluetoothMaster->InstallService();
    _powerManager->AddService(_bluetoothMaster, { Power::EPowerState::PluggedIn });

    _gpsService = new GPS::GPSService();
    _gpsService->InstallService();
    _powerManager->AddService(_gpsService, { Power::EPowerState::PluggedIn, Power::EPowerState::BatteryNormal, Power::EPowerState::BatteryLow });

    _gsmService = new GSM::GSMService();
    _gsmService->InstallService();
    //TODO: Set this module to power down when not in use (have the service keep handles of acquired instances for this).
    _powerManager->AddService(_gsmService, { Power::EPowerState::PluggedIn, Power::EPowerState::BatteryNormal, Power::EPowerState::BatteryLow });
    #pragma endregion
}

void loop()
{
    //I don't need this loop method as this program is task based.
    vTaskDelete(NULL);
}
