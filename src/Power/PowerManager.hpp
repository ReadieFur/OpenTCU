#pragma once

#include "Abstractions/AService.hpp"
#include "EPowerState.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mutex>
#include <WiFi.h>
#include <map>
#include <list>

class PowerManager : public AService
{
private:
    static const int TASK_STACK_SIZE = configIDLE_TASK_STACK_SIZE * 1.25;
    static const int TASK_PRIORITY = configMAX_PRIORITIES * 0.05;

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
        }
    }

    static int ManageService(AService* service, std::list<EPowerState> enabledStates, EPowerState currentPowerState)
    {
        if (std::find(enabledStates.begin(), enabledStates.end(), currentPowerState) != enabledStates.end())
            return service->StartService();
        else
            return service->StopService();
    }

    int InstallServiceImpl() override
    {
        WiFi.mode(WIFI_OFF);
        WiFi.setSleep(true);
        return 0;
    }

    int UninstallServiceImpl() override
    {
        return 0;
    }

    int StartServiceImpl() override
    {
        ESP_RETURN_ON_FALSE(xTaskCreate(Task, "PowerManagerTask", TASK_STACK_SIZE, this, TASK_PRIORITY, &_taskHandle) == pdPASS,
            1, nameof(PowerManager), "Failed to create power manager task.");
        return 0;
    }

    int StopServiceImpl() override
    {
        vTaskDelete(_taskHandle);
        return 0;
    }

    void UpdatePowerState()
    {
        //TODO: Implement.
    }

public:
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
        int retVal = IsRunning() ? ManageService(service, enabledStates, _powerState) : 0;

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
