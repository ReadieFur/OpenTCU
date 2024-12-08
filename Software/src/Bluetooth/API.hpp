#pragma once

#include <Service/AService.hpp>
#include "BLE.hpp"
#include "SGattServerProfile.h"
#include <esp_err.h>
#include "GattServerService.hpp"
#include <vector>

namespace ReadieFur::OpenTCU::Bluetooth
{
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

            GattServerService testService(ESP_GATT_UUID_DEVICE_INFO_SVC, 0);
            uint16_t testValue = 2;
            testService.AddAttribute(0x2A24, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, &testValue, sizeof(testValue), sizeof(uint16_t));
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
