#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include "pch.h"
#include "Config/Device.h"
#include "CAN/BusMaster.hpp"
#include "Power/PowerManager.hpp"
#include "Bluetooth/BluetoothMaster.hpp"
#include "Bluetooth/BleApi.hpp"
#include "GPS/GPSService.hpp"
#include "Networking/GSMService.hpp"
#include "Config/JsonFlash.hpp"
#include <esp_sleep.h>

#define ABORT_ON_FAIL(a, format) do {                                                   \
        if (unlikely(!(a))) {                                                           \
            ESP_LOGE("main", "%s(%d): " format ": %i", __FUNCTION__, __LINE__, a);      \
            abort();                                                                    \
        }                                                                               \
    } while(0)

using namespace ReadieFur::OpenTCU;

Config::JsonFlash* _config;
Power::PowerManager* _powerManager;
CAN::BusMaster* _busMaster;
Bluetooth::BluetoothMaster* _bluetoothMaster;
Bluetooth::BleApi* _bleApi;
GPS::GPSService* _gpsService;
Networking::GSMService* _gsmService;

void setup()
{
    _config = Config::JsonFlash::Open("config.json");
    ABORT_ON_FAIL(_config != nullptr, "Failed to open config " nameof(Config::JsonFlash) " service");

    _powerManager = new Power::PowerManager();
    ABORT_ON_FAIL(_powerManager->InstallService(), "Failed to install " nameof(Power::PowerManager) " service");
    ABORT_ON_FAIL(_powerManager->StartService(), "Failed to start " nameof(Power::PowerManager) " service");
    _powerManager->OnPowerStateChanged.Add([](Power::EPowerState powerState)
    {
        if (powerState != Power::EPowerState::BatteryCritical)
            return;

        //Shutdown all services.
        _powerManager->UninstallService();
        _gsmService->UninstallService();
        _gpsService->UninstallService();
        _bleApi->UninstallService();
        _bluetoothMaster->UninstallService();
        _busMaster->UninstallService();
        _config->~JsonFlash();

        //Enter deep sleep.
        esp_sleep_enable_timer_wakeup(BAT_CRIT_INT * 1000);
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
        esp_deep_sleep_start();
    });

    _busMaster = new CAN::BusMaster(_config);
    ABORT_ON_FAIL(_busMaster->InstallService(), "Failed to install " nameof(CAN::BusMaster) " service");
    ABORT_ON_FAIL(_powerManager->AddService(_busMaster, { Power::EPowerState::PluggedIn }), "Failed to start " nameof(CAN::BusMaster) " service");

    //TODO: Log these if they fail and retry, but don't abort.
    _bluetoothMaster = new Bluetooth::BluetoothMaster();
    _bluetoothMaster->InstallService();
    _powerManager->AddService(_bluetoothMaster, { Power::EPowerState::PluggedIn });

    _bleApi = new Bluetooth::BleApi();
    _bleApi->AddDependency(_bluetoothMaster);
    _bleApi->InstallService();
    _powerManager->AddService(_bleApi, { Power::EPowerState::PluggedIn });

    _gpsService = new GPS::GPSService();
    _gpsService->InstallService();
    _powerManager->AddService(_gpsService, { Power::EPowerState::PluggedIn, Power::EPowerState::BatteryNormal, Power::EPowerState::BatteryLow });

    _gsmService = new Networking::GSMService(_config);
    _gsmService->InstallService();
    //TODO: Set this module to power down when not in use (have the service keep handles of acquired instances for this).
    _powerManager->AddService(_gsmService, { Power::EPowerState::PluggedIn, Power::EPowerState::BatteryNormal, Power::EPowerState::BatteryLow });
}

void loop()
{
    //I don't need this loop method as this program is task based.
    vTaskDelete(NULL);
}
