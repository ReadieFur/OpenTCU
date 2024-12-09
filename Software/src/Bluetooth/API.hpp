#pragma once

#include <Service/AService.hpp>
#include "BLE.hpp"
#include "SGattServerProfile.h"
#include <esp_err.h>
#include "GattServerService.hpp"
#include <vector>

namespace ReadieFur::OpenTCU::Bluetooth
{
    //https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Assigned_Numbers/out/en/Assigned_Numbers.pdf?v=1733673361043
    class API : public Service::AService
    {
    private:
        std::vector<GattServerService*> _services;

        void ServerAppCallback(esp_gatts_cb_event_t event, esp_gatt_if_t gattsIf, esp_ble_gatts_cb_param_t* param)
        {
            for (auto &&service : _services)
                service->ProcessServerEvent(event, gattsIf, param);
        }

    protected:
        void RunServiceImpl() override
        {
            esp_err_t err;

            BLE* bleService = GetService<BLE>();

            //https://www.gofakeit.com/funcs/uint32
            //https://www.rapidtables.com/convert/number/decimal-to-hex.html
            GattServerService testService(SUUID(0x98C220BEUL), 0);
            uint16_t testValue = 53;
            uint16_t testValue2 = 25;
            testService.AddAttribute(SUUID(0xCAE35F80UL), ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, &testValue, sizeof(testValue), sizeof(uint16_t));
            testService.AddAttribute(SUUID(0x38B18BECUL), ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t),
            [testValue2](auto outValue, auto outLength)
            {
                LOGD(nameof(Bluetooth::API), "Manual read callback.");
                *outValue = testValue2;
                *outLength = sizeof(testValue2);
                return ESP_GATT_OK;
            },
            [&testValue2](auto inValue, auto inLength)
            {
                LOGD(nameof(Bluetooth::API), "Manual write callback.");
                testValue2 = *(uint16_t*)inValue;
                return ESP_GATT_OK;
            });
            _services.push_back(&testService);

            SGattServerProfile serverProfile =
            {
                .appId = 0x01,
                .gattServerCallback = [this](auto a, auto b, auto c){ ServerAppCallback(a, b, c); },
            };
            if ((err = bleService->RegisterServerApp(&serverProfile)) != ESP_OK)
            {
                LOGE(nameof(Bluetooth::API), "Failed to register server app: %s", esp_err_to_name(err));
                return;
            }

            LOGD(nameof(Bluetooth::API), "BLE API started.");
            ServiceCancellationToken.WaitForCancellation();

            bleService->UnregisterServerApp(serverProfile.appId);

            for (auto &&service : _services)
                delete service;
            _services.clear();
        }

    public:
        API()
        {
            ServiceEntrypointStackDepth += 1024;
            AddDependencyType<BLE>();
        }
    };
};
