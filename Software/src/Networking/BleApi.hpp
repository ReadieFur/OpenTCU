#pragma once

/** UUID format:
 * [Prefix(4)][Svc(4)]-[Mid1(4)]-[Mid2(4)]-[Char(4)]-[Suffix(12)]
 * Prefix(4) and Mid(8) should be obtained from a UUID generator to ensure uniqueness.
 * Svc(4) and Char(4) should be the user set identifiers for the service and characteristics respectively.
 * Suffix(12) can be customised for anything (branding, easter-egg, uuid or blank).
 */
#define UUID_PREFIX "ae74"
#define UUID_MID1 "8322"
#define UUID_MID2 "4e78"
#define UUID_SUFFIX "000000000000"
#define MAKE_UUID(svc, chr) UUID_PREFIX svc "-" UUID_MID1 "-" UUID_MID2 "-" chr "-" UUID_SUFFIX

#define MAIN_SERVICE_UUID MAKE_UUID("0001", "0000")
#define RUNTIME_CHAR_UUID MAKE_UUID("0001", "1001")
#define PERSISTENT_CHAR_UUID MAKE_UUID("0001", "1002")

#define UUID_DEBUG_SERVICE MAKE_UUID("0002", "0000")
#define INJECT_CHAR_UUID MAKE_UUID("0002", "2001")
#define REBOOT_CHAR_UUID MAKE_UUID("0002", "2002")

#include "ProgramConfig.h"
#include <Service/AService.hpp>
#include <NimBLEDevice.h>
#include <esp_err.h>
#include <functional>
#include <vector>
#include <string>
#include <cstring>
#include "CAN/BusMaster.hpp"
#include "Data/Persistent.hpp"
#include "Data/Live.hpp"

namespace ReadieFur::OpenTCU::Networking
{
    // https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Assigned_Numbers/out/en/Assigned_Numbers.pdf?v=1733673361043
    class BleApi : public Service::AService
    {
    private:
        class ServerCallbacks : public NimBLEServerCallbacks
        {
            public:
                struct SParams
                {
                    std::function<void(NimBLEServer* server, NimBLEConnInfo& connInfo)> OnConnect = nullptr;
                    std::function<void(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason)> OnDisconnect = nullptr;
                    std::function<void(uint16_t MTU, NimBLEConnInfo& connInfo)> OnMTUChange = nullptr;
                    std::function<uint32_t()> OnPassKeyDisplay = nullptr;
                    std::function<void(NimBLEConnInfo& connInfo)> OnPassKeyEntry = nullptr;
                    std::function<void(NimBLEConnInfo& connInfo, uint32_t pin)> OnConfirmPassKey = nullptr;
                    std::function<void(NimBLEConnInfo& connInfo)> OnAuthenticationComplete = nullptr;
                    std::function<void(NimBLEConnInfo& connInfo)> OnIdentity = nullptr;
                    std::function<void(NimBLEConnInfo& connInfo)> OnConnParamsUpdate = nullptr;
                };

            private:
                SParams _callbacks;

            public:
                ServerCallbacks(const SParams& callbacks) : _callbacks(callbacks) {}

                void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override { if (_callbacks.OnConnect) _callbacks.OnConnect(server, connInfo); }
                void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override { if (_callbacks.OnDisconnect) _callbacks.OnDisconnect(server, connInfo, reason); }
                void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override { if (_callbacks.OnMTUChange) _callbacks.OnMTUChange(MTU, connInfo); }
                uint32_t onPassKeyDisplay() override { return _callbacks.OnPassKeyDisplay ? _callbacks.OnPassKeyDisplay() : 0; }
                void onPassKeyEntry(NimBLEConnInfo& connInfo) override { if (_callbacks.OnPassKeyEntry) _callbacks.OnPassKeyEntry(connInfo); }
                void onConfirmPassKey(NimBLEConnInfo& connInfo, uint32_t pin) override { if (_callbacks.OnConfirmPassKey) _callbacks.OnConfirmPassKey(connInfo, pin); }
                void onAuthenticationComplete(NimBLEConnInfo& connInfo) override { if (_callbacks.OnAuthenticationComplete) _callbacks.OnAuthenticationComplete(connInfo); }
                void onIdentity(NimBLEConnInfo& connInfo) override { if (_callbacks.OnIdentity) _callbacks.OnIdentity(connInfo); }
                void onConnParamsUpdate(NimBLEConnInfo& connInfo) override { if (_callbacks.OnConnParamsUpdate) _callbacks.OnConnParamsUpdate(connInfo); }
        };

        class CharacteristicCallback : public NimBLECharacteristicCallbacks
        {
            public:
                struct SParams
                {
                    std::function<void(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo)> OnRead = nullptr;
                    std::function<void(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo)> OnWrite = nullptr;
                    std::function<void(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo, int code)> OnStatus = nullptr;
                    std::function<void(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo, uint16_t subValue)> OnSubscribe = nullptr;
                };

            private:
                SParams _callbacks;
 
            public:
                CharacteristicCallback(const SParams& callbacks) : _callbacks(callbacks) {}

                void onRead(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override { if (_callbacks.OnRead) _callbacks.OnRead(characteristic, connInfo); }
                void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override { if (_callbacks.OnWrite) _callbacks.OnWrite(characteristic, connInfo); }
                void onStatus(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo, int code) override { if (_callbacks.OnStatus) _callbacks.OnStatus(characteristic, connInfo, code); }
                void onSubscribe(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override { if (_callbacks.OnSubscribe) _callbacks.OnSubscribe(characteristic, connInfo, subValue); }
        };

        struct __attribute__((packed)) SPersistentDataPayload
        {
            uint16_t BaseWheelCircumference;
            uint16_t TargetWheelCircumference;
        };

        static const uint NOTIFY_TASK_STACK_SIZE = CONFIG_FREERTOS_IDLE_TASK_STACKSIZE + 1024;
        static const uint NOTIFY_TASK_PRIORITY = configMAX_PRIORITIES * 0.2;
        static const uint NOTIFY_TASK_INTERVAL_MS = 500;

        CAN::BusMaster* _busMaster = nullptr;
        TaskHandle_t _notifyTaskHandle = NULL;
        NimBLEAdvertising* _advertising = nullptr;
        NimBLECharacteristic* _runtimeStatsCharacteristic = nullptr;

        void OnServerConnect(NimBLEServer* server, NimBLEConnInfo& connInfo)
        {
            LOGI(nameof(Networking::BleApi), "Client connected: %s", connInfo.getAddress().toString().c_str());
        }

        void OnServerDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason)
        {
            if (_advertising)
                _advertising->start();
        }

        void SyncRuntimeStats(NimBLECharacteristic* characteristic, const Data::SLive& snapshot)
        {
            characteristic->setValue(reinterpret_cast<const uint8_t*>(&snapshot), sizeof(Data::SLive));
        }

        void NotifyTask()
        {
            Data::SLive lastLiveData = {};

            while (ServiceCancellationToken.IsCancellationRequested())
            {
                Data::SLive liveDataSnapshot = Data::Live::Current;

                // Since SLive uses __attribute__((packed)) we can compare the memory spaces directly.
                if (std::memcmp(&lastLiveData, &liveDataSnapshot, sizeof(Data::SLive)) != 0)
                {
                    lastLiveData = liveDataSnapshot;

                    SyncRuntimeStats(_runtimeStatsCharacteristic, liveDataSnapshot);
                    _runtimeStatsCharacteristic->notify();
                }

                vTaskDelay(pdMS_TO_TICKS(NOTIFY_TASK_INTERVAL_MS));
            }

            vTaskDelete(NULL);
        }

        void OnRuntimeStatsRead(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo)
        {
            Data::SLive liveDataSnapshot = Data::Live::Current;
            SyncRuntimeStats(characteristic, liveDataSnapshot);
        }

        void OnPersistentDataRead(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo)
        {
            SPersistentDataPayload payload = {
                .BaseWheelCircumference = Data::Persistent::BaseWheelCircumference,
                .TargetWheelCircumference = Data::Persistent::TargetWheelCircumference
            };
            characteristic->setValue(reinterpret_cast<const uint8_t*>(&payload), sizeof(SPersistentDataPayload));
        }

        void OnPersistentDataWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo)
        {
            std::string value = characteristic->getValue();
            if (value.length() < sizeof(SPersistentDataPayload))
            {
                LOGW(nameof(Networking::BleApi), "Persistent write rejected: Data too short.");
                return;
            }

            // Map the raw bytes to the typed struct.
            const SPersistentDataPayload* incoming = reinterpret_cast<const SPersistentDataPayload*>(value.data());
            bool hasChanges = false;

            if (incoming->BaseWheelCircumference >= 800 && incoming->BaseWheelCircumference <= 2400) {
                if (Data::Persistent::BaseWheelCircumference != incoming->BaseWheelCircumference) {
                    LOGI(nameof(Networking::BleApi), "Updating Base Wheel: %u", incoming->BaseWheelCircumference);
                    Data::Persistent::BaseWheelCircumference = incoming->BaseWheelCircumference;
                    hasChanges = true;
                }
            }

            if (incoming->TargetWheelCircumference >= 800 && incoming->TargetWheelCircumference <= 2400) {
                if (Data::Persistent::TargetWheelCircumference != incoming->TargetWheelCircumference) {
                    LOGI(nameof(Networking::BleApi), "Updating Target Wheel: %u", incoming->TargetWheelCircumference);
                    _busMaster->SetTargetWheelCircumference(incoming->TargetWheelCircumference);
                    hasChanges = true;
                }
            }

            if (hasChanges)
                Data::Persistent::Save();
        }

        void OnInjectWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo)
        {
            const size_t injectMessageDataSize =
                sizeof(uint8_t) //bus
                + sizeof(uint32_t) //id
                + sizeof(uint8_t) //length
                + (sizeof(uint8_t) * 8); //data

            // Validate incoming data size.
            std::string value = characteristic->getValue(); 
            const uint8_t* inValue = (const uint8_t*)value.data();
            uint16_t inLength = value.length();
            if (inLength != injectMessageDataSize)
            {
                LOGW(nameof(Networking::BleApi), "Received invalid inject message with length %d (expected %d)", inLength, injectMessageDataSize);
                return;
            }

            // Build CAN frame.
            bool bus = inValue[0];
            CAN::SCanMessage message = {
                .id = (uint32_t)(inValue[1] | inValue[2] << 8 | inValue[3] << 16 | inValue[4] << 24),
                .length = inValue[5],
                .isExtended = false, // You might want to pull this from a bit in the ID or a new flag byte
                .isRemote = false
            };

            // Validate CAN frame.
            if (message.length > 8)
            {
                LOGW(nameof(Networking::BleApi), "Received invalid inject message with length %d (max 8)", message.length);
                return;
            }

            // Copy data part into CAN frame.
            for (size_t i = 0; i < message.length; i++)
                message.data[i] = inValue[6 + i];

            // Inject message into bus.
            // _busMaster should always be valid here since it's set before the BLE server is created and this is dereferenced before the instance is disposed.
            esp_err_t res = _busMaster->InjectMessage(bus, message);
            if (res != ESP_OK)
                LOGE(nameof(Networking::BleApi), "Failed to inject message: %i", res);
        }

        void OnRebootWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo)
        {
            LOGI(nameof(Networking::BleApi), "Rebooting device.");
            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();
        }

    protected:
        void RunServiceImpl() override
        {
            // Shouldn't ever be null as we set it as a dependency, service installer will fail if it is.
            _busMaster = GetService<CAN::BusMaster>();

            // If the bike serial number is empty, wait for a short period to attempt to acquire it from the TCU before defaulting to using the MAC address as the device name.
            Data::Persistent::WaitForDeviceName(pdMS_TO_TICKS(5000));
            NimBLEDevice::init(Data::Persistent::DeviceName);
            // NimBLEDevice::setMTU(512); // Allow larger packets to be sent.

            /* TODO:
             * Update security so that the device can connect without a passkey
             * but keep functions disabled until OpenTCU has validated a connection to the MastermindTCU
             * this validates that the chip is installed on a bike the user owns.
             * Use NIMBLE_PROPERTY::READ_AUTHEN for locked properties.
             */
            #ifdef TCU_CODE
            NimBLEDevice::setSecurityAuth(true, true, true); // Bonding, MITM, SC
            NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY); // Set IO to "Display Only" so the phone knows it must provide a keyboard to enter the PIN
            NimBLEDevice::setSecurityPasskey(Data::Persistent::Pin);
            #endif

            NimBLEServer* bleServer = NimBLEDevice::createServer();
            bleServer->setCallbacks(new ServerCallbacks({
                .OnConnect = std::bind(&BleApi::OnServerConnect, this, std::placeholders::_1, std::placeholders::_2),
                .OnDisconnect = std::bind(&BleApi::OnServerDisconnect, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
            }));

            NimBLEService* mainService = bleServer->createService(MAIN_SERVICE_UUID);
            _runtimeStatsCharacteristic = mainService->createCharacteristic(RUNTIME_CHAR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
            _runtimeStatsCharacteristic->setCallbacks(new CharacteristicCallback({.OnRead = std::bind(&BleApi::OnRuntimeStatsRead, this, std::placeholders::_1, std::placeholders::_2)}));
            NimBLECharacteristic* persistentDataCharacteristic = mainService->createCharacteristic(PERSISTENT_CHAR_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE /*| NIMBLE_PROPERTY::NOTIFY*/); // TODO: Implement notify for this characteristic.
            persistentDataCharacteristic->setCallbacks(new CharacteristicCallback({
                .OnRead = std::bind(&BleApi::OnPersistentDataRead, this, std::placeholders::_1, std::placeholders::_2),
                .OnWrite = std::bind(&BleApi::OnPersistentDataWrite, this, std::placeholders::_1, std::placeholders::_2)
            }));

            NimBLEService* debugService = bleServer->createService(UUID_DEBUG_SERVICE);
            NimBLECharacteristic* injectCharacteristic = debugService->createCharacteristic(INJECT_CHAR_UUID, NIMBLE_PROPERTY::WRITE);
            injectCharacteristic->setCallbacks(new CharacteristicCallback({.OnWrite = std::bind(&BleApi::OnInjectWrite, this, std::placeholders::_1, std::placeholders::_2)}));
            NimBLECharacteristic* rebootCharacteristic = debugService->createCharacteristic(REBOOT_CHAR_UUID, NIMBLE_PROPERTY::WRITE);
            rebootCharacteristic->setCallbacks(new CharacteristicCallback({.OnWrite = std::bind(&BleApi::OnRebootWrite, this, std::placeholders::_1, std::placeholders::_2)}));

            if (xTaskCreate([](void* param) { static_cast<BleApi*>(param)->NotifyTask(); }, "BleNotifyTask", NOTIFY_TASK_STACK_SIZE, this, NOTIFY_TASK_PRIORITY, &_notifyTaskHandle) != pdPASS)
            {
                LOGE(nameof(Networking::BleApi), "Failed to create task.");
                return;
            }

            _advertising = NimBLEDevice::getAdvertising();
            _advertising->setName(Data::Persistent::DeviceName);
            _advertising->addServiceUUID(MAIN_SERVICE_UUID);
            _advertising->start();

            LOGD(nameof(Networking::BleApi), "BLE API started.");
            ServiceCancellationToken.WaitForCancellation();

            _advertising->stop();
            bleServer->removeService(mainService);
            bleServer->removeService(debugService);
        }

    public:
        BleApi()
        {
            ServiceEntrypointStackDepth += 1024;
            AddDependencyType<CAN::BusMaster>();
        }
    };
};
