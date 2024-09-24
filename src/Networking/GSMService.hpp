#pragma once

#include "Abstractions/AService.hpp"
#include "pch.h"
#include <SoftwareSerial.h>
#include <TinyGSM.h>
#include <Arduino.h>
#include <esp_log.h>
#include <map>
#include <Client.h>
#include <mutex>
#include <set>
#include "Config/JsonFlash.hpp"
#ifdef DUMP_GSM_SERIAL
#include <StreamDebugger.h>
#endif

#define TINY_GSM_DEBUG Serial
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false

namespace ReadieFur::OpenTCU::Networking
{
    class GSMService : public Abstractions::AService
    {
    private:
        static const int TASK_STACK_SIZE = configIDLE_TASK_STACK_SIZE * 5; //In my testing TinyGSM needs a massive stack.
        static const int TASK_PRIORITY = configMAX_PRIORITIES * 0.2;
        static const int TASK_INTERVAL = pdMS_TO_TICKS(5000);

        Config::JsonFlash* _config;
        SoftwareSerial _gsmSerial;
        #ifdef DUMP_GSM_SERIAL
        StreamDebugger* _debugger;
        #endif
        TinyGsm* _modem;
        std::mutex _clientsMutex;
        std::map<TinyGsmClient*, ushort> _clients;
        TaskHandle_t _taskHandle = NULL;

        static void Task(void* param)
        {
            GSMService* self = static_cast<GSMService*>(param);

            while (eTaskGetState(NULL) != eTaskState::eDeleted)
            {
                bool holdsActiveReferences = true;
                for (auto &&reference : self->_referencedBy)
                {
                    if (holdsActiveReferences = reference->IsRunning())
                        break;
                }

                self->_modem->sleepEnable(!holdsActiveReferences);

                if (holdsActiveReferences)
                    self->Connect();
                
                vTaskDelay(TASK_INTERVAL);
            }
        }

        bool Connect()
        {
            //Make sure we're still registered on the network.
            if (_modem->isNetworkConnected())
                return true;

            ESP_LOGI(nameof(GSMService), "GSM disconnected. Reconnecting...");
            if (!_modem->waitForNetwork(60000L, true))
            {
                ESP_LOGI(nameof(GSMService), "GSM reconnect: Failed");
                return false;
            }
            if (_modem->isNetworkConnected())
            {
                ESP_LOGV(nameof(GSMService), "GSM reconnect: Success");
            }

            //And make sure GPRS/EPS is still connected.
            if (_modem->isGprsConnected())
                return true;

            String apn = "", username = "", password = "";
            _config->TryGet("gsm_apn", apn);
            if (apn.isEmpty())
                return false;
            _config->TryGet("gsm_username", username);
            _config->TryGet("gsm_password", password);
            ESP_LOGI(nameof(GSMService), "GPRS disconnected. Reconnecting...");
            if (!_modem->gprsConnect(apn.c_str(), username.c_str(), password.c_str()))
            {
                ESP_LOGI(nameof(GSMService), "GPRS reconnect: Failed");
                return false;
            }
            if (_modem->isGprsConnected())
            {
                ESP_LOGV(nameof(GSMService), "GPRS reconnect: Success");
            }
        }

    protected:
        int InstallServiceImpl() override
        {
            if constexpr (GSM_TX_PIN == GPIO_NUM_NC || GSM_RX_PIN == GPIO_NUM_NC)
                return 1;

            #pragma region Setup IO
            pinMode(GSM_RX_PIN, INPUT_PULLDOWN);
            pinMode(GSM_TX_PIN, OUTPUT);
            if constexpr (GSM_PWR_PIN != GPIO_NUM_NC)
            {
                //Power on modem.
                pinMode(GSM_PWR_PIN, OUTPUT);
                digitalWrite(GSM_PWR_PIN, HIGH);
            }
            if constexpr (GSM_RST_PIN != GPIO_NUM_NC)
            {
                //Reset modem.
                pinMode(GSM_RST_PIN, OUTPUT);
                digitalWrite(GSM_RST_PIN, LOW);
            }
            if constexpr (GSM_PWRKEY_PIN != GPIO_NUM_NC)
            {
                //Boot sequence (board specific).
                pinMode(GSM_PWRKEY_PIN, OUTPUT);
                digitalWrite(GSM_PWRKEY_PIN, LOW);
                vTaskDelay(pdMS_TO_TICKS(100));
                digitalWrite(GSM_PWRKEY_PIN, HIGH);
                vTaskDelay(pdMS_TO_TICKS(1000));
                digitalWrite(GSM_PWRKEY_PIN, LOW);
            }
            if constexpr (GSM_RING_PIN != GPIO_NUM_NC)
                pinMode(GSM_RING_PIN, INPUT_PULLUP);

            _gsmSerial.begin(GSM_BAUD, SWSERIAL_8N1, GSM_RX_PIN, GSM_TX_PIN);

            #ifdef DUMP_GSM_SERIAL
            _debugger = new StreamDebugger(_gsmSerial, Serial);
            _modem = new TinyGsm(_debugger);
            #else
            _modem = new TinyGsm(_gsmSerial);
            #endif
            #pragma endregion

            #pragma region Configure device
            vTaskDelay(3000);
            //Restart takes quite some time, to skip it, call init() instead of restart().
            ESP_RETURN_ON_FALSE(_modem->init(), 2, nameof(GSMService), "Failed to start modem.");

            ESP_LOGV(nameof(GSMService), "Modem Name: %s", _modem->getModemName());
            ESP_LOGV(nameof(GSMService), "Modem Info: %s", _modem->getModemInfo());

            //Unlock your SIM card with a PIN if needed.
            String pin;
            if (_config->TryGet("gsm_pin", pin) && !pin.isEmpty() && _modem->getSimStatus() != 3)
                ESP_RETURN_ON_FALSE(_modem->simUnlock(pin.c_str()), 3, nameof(GSMService), "Failed to unlock SIM card.");
            #pragma endregion

            //Put the module to sleep while it is not in use.
            _modem->sleepEnable(true);

            return 0;
        };

        int UninstallServiceImpl() override
        {
            if constexpr (GSM_TX_PIN == GPIO_NUM_NC || GSM_RX_PIN == GPIO_NUM_NC)
                return 0;

            for (auto &&client : _clients)
                DestroyClient(client.first);
            
            delete _modem;
            #ifdef DUMP_GSM_SERIAL
            delete _debugger;
            #endif

            return 0;
        };

        int StartServiceImpl() override
        {
            if constexpr (GSM_TX_PIN == GPIO_NUM_NC || GSM_RX_PIN == GPIO_NUM_NC)
                return 1;

            _modem->sleepEnable(false);

            return 0;
        };

        int StopServiceImpl() override
        {
            if constexpr (GSM_TX_PIN == GPIO_NUM_NC || GSM_RX_PIN == GPIO_NUM_NC)
                return 0;

            _modem->sleepEnable(true);

            return 0;
        };

    public:
        GSMService(Config::JsonFlash* config) : _config(config) {}

        TinyGsmClient* CreateClient()
        {
            if (!IsRunning())
                return nullptr;

            _clientsMutex.lock();

            if (_clients.size() >= TINY_GSM_MUX_COUNT)
            {
                ESP_LOGE(nameof(GSMService), "Max number of GPRS clients already instantiated.");
                _clientsMutex.unlock();
                return nullptr;
            }

            //Find the first free ID to use for the mux value.
            ushort firstFreeId = 0;
            std::set<ushort> usedIds;
            for (auto &&client : _clients)
                usedIds.insert(client.second);
            while (usedIds.find(firstFreeId) != usedIds.end())
                firstFreeId++;

            //firstFreeId shouldn't exceed TINY_GSM_MUX_COUNT here.

            TinyGsmClient* client = new TinyGsmClient(*_modem, firstFreeId);
            if (client == nullptr)
            {
                ESP_LOGE(nameof(GSMService), "Failed to create GSM client object.");
                _clientsMutex.unlock();
                return nullptr;
            }

            _clients.insert({client, firstFreeId});
            _clientsMutex.unlock();
            return client;
        }

        void DestroyClient(TinyGsmClient* client)
        {
            if (!IsRunning())
                return;

            _clientsMutex.lock();
            _clients.erase(client);
            _clientsMutex.unlock();
            delete client;
        }
    };
};
