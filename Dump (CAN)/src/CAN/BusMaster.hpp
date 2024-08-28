#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/twai.h>
#include "ACan.h"
#include "TwaiCan.hpp"
#include "Logging.h"
#ifdef ENABLE_CAN_DUMP
#include <vector>
#include <time.h>
#endif

#define CAN_TIMEOUT_TICKS pdMS_TO_TICKS(50)
#define RELAY_TASK_STACK_SIZE 1024 * 2.5
#define RELAY_TASK_PRIORITY configMAX_PRIORITIES - 10

class BusMaster
{
private:
    static bool _running; //I should add a semaphore here but it's not worth it for now.

    static void RelayTask(void* param)
    {
        ACan* canBus = static_cast<ACan*>(param);

        while (true)
        {
            SCanMessage message;
            // TRACE("Waiting for message");
            if (esp_err_t receiveResult = canBus->Receive(&message, CAN_TIMEOUT_TICKS) == ESP_OK)
            {
                #ifdef ENABLE_CAN_DUMP
                SCanDump dump = {
                    .isSpi = false,
                    .message = message
                };

                struct tm timeInfo;
                if (getLocalTime(&timeInfo))
                {
                    //Get Epoch time.
                    time(&dump.timestamp);
                    dump.timestamp2 = xTaskGetTickCount();
                }
                else
                {
                    dump.timestamp = 0;
                    dump.timestamp2 = xTaskGetTickCount();
                }

                if (BaseType_t queueResult = xQueueSend(canDumpQueue, &dump, 0) != pdTRUE)
                {
                    LOG_WARN("Failed to add to message '%x' to dump queue: %d", message.id, queueResult);
                }
                #endif
            }
            else
            {
                // TRACE("Timeout: %x", receiveResult);
            }
        }
    }

    static void Init()
    {
        #pragma region Setup TWAI CAN
        //Configure GPIO.
        gpio_config_t txPinConfig = {
            .pin_bit_mask = 1ULL << TWAI_TX_PIN,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config_t rxPinConfig = {
            .pin_bit_mask = 1ULL << TWAI_RX_PIN,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        ASSERT(gpio_config(&txPinConfig) == ESP_OK);
        ASSERT(gpio_config(&rxPinConfig) == ESP_OK);

        twaiCan = new TwaiCan(
            TWAI_GENERAL_CONFIG_DEFAULT(
                TWAI_TX_PIN,
                TWAI_RX_PIN,
                TWAI_MODE_NORMAL
            ),
            TWAI_TIMING_CONFIG_250KBITS(),
            TWAI_FILTER_CONFIG_ACCEPT_ALL()
        );
        #pragma endregion
    }

    static void Start()
    {
        if (_running)
            return;
        _running = true;
        #pragma region CAN task setup
        //Create high priority tasks to handle CAN relay tasks.
        //I am creating the parameters on the heap just in case this method returns before the task starts which will result in an error.
        //TODO: Determine if I should run both CAN tasks on one core and do secondary processing (i.e. metrics, user control, etc) on the other core, or split the load between all cores with CAN bus getting their own core.
        ASSERT(
            xTaskCreate(
                RelayTask, "TwaiCan", RELAY_TASK_STACK_SIZE, twaiCan, RELAY_TASK_PRIORITY, &twaiTaskHandle
            ) == pdPASS);
        #pragma endregion
    }

    static void Destroy()
    {
        if (_running)
            Stop();

        delete twaiCan;
        twaiCan = nullptr;
    }

public:
    static TwaiCan* twaiCan;
    static TaskHandle_t twaiTaskHandle;
    #ifdef ENABLE_CAN_DUMP
    static QueueHandle_t canDumpQueue;
    struct SCanDump
    {
        long timestamp;
        long timestamp2;
        bool isSpi;
        SCanMessage message;
    };
    #endif

    static void Begin()
    {
        if (_running)
            return;

        Init();
        Start();
    }

    static void Stop()
    {
        if (!_running)
            return;

        vTaskDelete(twaiTaskHandle);

        _running = false;
    }
};

bool BusMaster::_running = false;
TwaiCan* BusMaster::twaiCan = nullptr;
TaskHandle_t BusMaster::twaiTaskHandle;
#ifdef ENABLE_CAN_DUMP
QueueHandle_t BusMaster::canDumpQueue = xQueueCreate(500, sizeof(BusMaster::SCanDump));
#endif
