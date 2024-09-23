#pragma once

#include "Abstractions/AService.hpp"
#include "EPowerState.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mutex>
#include <WiFi.h>
#include <map>
#include <list>
#include <vector>
#include <algorithm>
#include <numeric>
#include "Abstractions/Event.hpp"

static_assert(BAT_CHG > BAT_LOW, nameof(BAT_CHG) " cannot be lower than " nameof(BAT_LOW) ".");
static_assert(BAT_CHG > BAT_CRIT, nameof(BAT_CHG) " cannot be lower than " nameof(BAT_CRIT) ".");
static_assert(BAT_LOW > BAT_CRIT, nameof(BAT_LOW) " cannot be lower than " nameof(BAT_CRIT) ".");

class PowerManager : public AService
{
private:
    static const int TASK_STACK_SIZE = configIDLE_TASK_STACK_SIZE * 1.25;
    static const int TASK_PRIORITY = configMAX_PRIORITIES * 0.05;
    static const int TASK_INTERVAL = pdMS_TO_TICKS(30 * 1000);

    std::mutex _serviceWatcherMutex;
    std::map<AService*, std::list<EPowerState>> _managedServices;
    TaskHandle_t _taskHandle = NULL;
    EPowerState _powerState = EPowerState::PluggedIn; //Assume plugged in by default.

    static void Task(void* param)
    {
        PowerManager* self = static_cast<PowerManager*>(param);

        while (true)
        {
            if (eTaskGetState(NULL) == eTaskState::eDeleted)
                break;

            EPowerState oldPowerState = self->_powerState;
            self->UpdatePowerState();
            
            for (auto &&kvp : self->_managedServices)
                self->ManageService(kvp.first);

            if (oldPowerState != self->_powerState)
                self->OnPowerStateChanged.Dispatch(self->_powerState);

            vTaskDelay(TASK_INTERVAL);
        }
    }

    int ManageService(AService* service)
    {
        int retVal;
        //No need to check if the service is already started before calling this as the start/stop methods will check this internally.
        if (std::find(_managedServices[service].begin(), _managedServices[service].end(), _powerState) != _managedServices[service].end())
        {
            _serviceWatcherMutex.lock();
            retVal = service->StartService();
            _serviceWatcherMutex.unlock();
        }
        else
        {
            _serviceWatcherMutex.lock();
            retVal = service->StopService();
            _serviceWatcherMutex.unlock();
        }
        return retVal;
    }

    int InstallServiceImpl() override
    {
        return 0;
    }

    int UninstallServiceImpl() override
    {
        return 0;
    }

    int StartServiceImpl() override
    {
        if constexpr (BAT_ADC == GPIO_NUM_NC)
        {
            //Board has not been configured with a battery, so don't start the task but return true and set the power mode to plugged in.
            _powerState = EPowerState::PluggedIn;
        }
        else
        {
            //Set the initial value.
            UpdatePowerState();

            ESP_RETURN_ON_FALSE(
                xTaskCreate(Task, "PowerManagerTask", TASK_STACK_SIZE, this, TASK_PRIORITY, &_taskHandle) == pdPASS,
                1, nameof(PowerManager), "Failed to create power manager task."
            );
        }

        return 0;
    }

    int StopServiceImpl() override
    {
        if constexpr (BAT_ADC != GPIO_NUM_NC)
            vTaskDelete(_taskHandle);
        return 0;
    }

    void UpdatePowerState()
    {
        //Get samples of power source voltage.
        std::vector<uint32_t> data;
        for (int i = 0; i < 30; ++i)
        {
            uint32_t val = analogReadMilliVolts(BAT_ADC);
            data.push_back(val);
            vTaskDelay(pdMS_TO_TICKS(30));
        }
        std::sort(data.begin(), data.end());
        data.erase(data.begin());
        data.pop_back();

        //Calculate the average power data.
        int sum = std::accumulate(data.begin(), data.end(), 0);
        double average = static_cast<double>(sum) / data.size();
        average *= 2;

        _serviceWatcherMutex.lock();
        if (average > BAT_CHG)
            _powerState = EPowerState::PluggedIn;
        else if (average > BAT_LOW)
            _powerState = EPowerState::BatteryNormal;
        else if (average > BAT_CRIT)
            _powerState = EPowerState::BatteryLow;
        else
            _powerState = EPowerState::BatteryCritical;
        _serviceWatcherMutex.unlock();
    }

public:
    Event<EPowerState> OnPowerStateChanged;

    PowerManager()
    {
        if constexpr (BAT_ADC != GPIO_NUM_NC)
            pinMode(BAT_ADC, INPUT_PULLDOWN);

        WiFi.mode(WIFI_OFF);
        WiFi.setSleep(true);
    }

    EPowerState GetPowerState()
    {
        return _powerState;
    }

    int AddService(AService* service, std::list<EPowerState> enabledStates)
    {
        if (service == nullptr)
            return -1;

        _serviceWatcherMutex.lock();

        _managedServices[service] = enabledStates;
        //Manage the service immediately (if the service is enabled).
        int retVal = IsRunning() ? ManageService(service) : 0;

        _serviceWatcherMutex.unlock();
        return retVal;
    }

    void RemoveService(AService* service)
    {
        if (service == nullptr)
            return;
        _serviceWatcherMutex.lock();
        _managedServices.erase(service);
        _serviceWatcherMutex.unlock();
    }
};
