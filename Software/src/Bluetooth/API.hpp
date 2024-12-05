#pragma once

#include <Service/AService.hpp>
#include "BLE.hpp"

namespace ReadieFur::OpenTCU::Bluetooth
{
    class API : public Service::AService
    {
    private:
        static const uint16_t APP_ID = 52876; //Just some random number: https://www.gofakeit.com/funcs/uint16

        void ServerAppCallback(esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param)
        {
        }

    protected:
        void RunServiceImpl() override
        {
            BLE* bleService = GetService<BLE>();

            bleService->RegisterServerApp(APP_ID, [this](auto a, auto b){ ServerAppCallback(a, b); });
            //TODO: Register services and characteristics.

            ServiceCancellationToken.WaitForCancellation();

            bleService->UnregisterServerApp(APP_ID);
        }

    public:
        API()
        {
            ServiceEntrypointStackDepth += 1024;
            AddDependencyType<BLE>();
        }
    };
};
