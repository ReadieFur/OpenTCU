#pragma once

#include "Abstractions/AService.hpp"
#include "pch.h"
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <Arduino.h>
#include "Abstractions/Event.hpp"

namespace ReadieFur::OpenTCU::GPS
{
    class GPSService : public Abstractions::AService
    {
    private:
        static const int TASK_STACK_SIZE = configIDLE_TASK_STACK_SIZE * 1.25;
        static const int TASK_PRIORITY = configMAX_PRIORITIES * 0.15;
        static const int TASK_INTERVAL = pdMS_TO_TICKS(100);

        SoftwareSerial _gpsSerial;
        TaskHandle_t _taskHandle = NULL;

        static void Task(void* param)
        {
            GPSService* self = static_cast<GPSService*>(param);

            while (eTaskGetState(NULL) != eTaskState::eDeleted)
            {
                while (self->_gpsSerial.available())
                {
                    int c = self->_gpsSerial.read();
                    #ifdef DUMP_GPS_SERIAL
                    Serial.write(c);
                    #endif
                    if (self->TinyGps.encode(c))
                        self->OnUpdated.Dispatch(true);
                }

                vTaskDelay(TASK_INTERVAL);
            }
        }

    protected:
        int InstallServiceImpl() override
        {
            //For this program the RX pin is the only one we care about.
            if constexpr (GPS_RX_PIN == GPIO_NUM_NC)
                return 0;
            
            pinMode(GPS_RX_PIN, INPUT_PULLDOWN);
            pinMode(GPS_TX_PIN, OUTPUT);
            _gpsSerial.begin(GPS_BAUD, SWSERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
            
            return 0;
        };

        int UninstallServiceImpl() override
        {
            if constexpr (GPS_RX_PIN != GPIO_NUM_NC)
                _gpsSerial.end();
            return 0;
        };

        int StartServiceImpl() override
        {
            if constexpr (GPS_RX_PIN != GPIO_NUM_NC)
                ESP_RETURN_ON_FALSE(
                    xTaskCreate(Task, "GpsTask", TASK_STACK_SIZE, this, TASK_PRIORITY, &_taskHandle) == pdPASS,
                    1, nameof(GPSService), "Failed to create GPS service task."
                );
            return 0;
        };

        int StopServiceImpl() override
        {
            if constexpr (GPS_RX_PIN != GPIO_NUM_NC)
                vTaskDelete(_taskHandle);
            return 0;
        };

    public:
        TinyGPSPlus TinyGps;
        Abstractions::Event<bool> OnUpdated;
    };
};
