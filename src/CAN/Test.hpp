#pragma once

#include "BusMaster.hpp"

namespace ReadieFur::OpenTCU::CAN
{
    class Test : public BusMaster
    {
    private:
        static const int TEST_CAN_TIMEOUT_TICKS = pdMS_TO_TICKS(500);

        static void TestTask(void* param)
        {
            SRelayTaskParameters* params = static_cast<SRelayTaskParameters*>(param);

            TaskHandle_t taskHandle = xTaskGetHandle(pcTaskGetName(NULL));
            uint32_t id = pcTaskGetName(taskHandle)[3] == '1' ? 0x1 : 0x2;

            //Check if the task has been signalled for deletion.
            while (eTaskGetState(taskHandle) != eTaskState::eDeleted)
            {
                //Generate a message to send.
                SCanMessage txMessage =
                {
                    .id = id,
                    .length = 1
                };
                txMessage.data[0] = xTaskGetTickCount();

                //Attempt to send a message to the bus.
                if (params->can1->Send(txMessage, TEST_CAN_TIMEOUT_TICKS) != ESP_OK)
                {
                    ESP_LOGW("Test", "%i failed to send message.", id);
                    continue;
                }

                //Attempt to read a message from the bus.
                SCanMessage rxMessage;
                if (esp_err_t receiveResult = params->can2->Receive(&rxMessage, TEST_CAN_TIMEOUT_TICKS) != ESP_OK)
                {
                    ESP_LOGW("Test", "%i failed to receive message.", id);
                    continue;
                }

                //Print the received message.
                ESP_LOGI("Test", "%i received message: %i, %i", id, rxMessage.id, rxMessage.data[0]);

                taskYIELD();
            }

            delete params;
        }

    protected:
        int StartServiceImpl() override
        {
            SRelayTaskParameters* params1 = new SRelayTaskParameters { this, _can1, _can2 };
            SRelayTaskParameters* params2 = new SRelayTaskParameters { this, _can2, _can1 };

            #if SOC_CPU_CORES_NUM > 1
            {
                ESP_RETURN_ON_FALSE(
                    xTaskCreatePinnedToCore(TestTask, "CAN1->CAN2", RELAY_TASK_STACK_SIZE, params1, RELAY_TASK_PRIORITY, &_can1TaskHandle, 0) == pdPASS,
                    2, nameof(BusMaster), "Failed to create relay task for CAN1->CAN2."
                );
                ESP_RETURN_ON_FALSE(
                    xTaskCreatePinnedToCore(TestTask, "CAN2->CAN1", RELAY_TASK_STACK_SIZE, params2, RELAY_TASK_PRIORITY, &_can2TaskHandle, 1) == pdPASS,
                    3, nameof(BusMaster), "Failed to create relay task for CAN2->CAN1."
                );
            }
            #else
            {
                ESP_RETURN_ON_FALSE(
                    xTaskCreate(TestTask, "CAN1->CAN2", RELAY_TASK_STACK_SIZE, params1, RELAY_TASK_PRIORITY, &_can1TaskHandle) == pdPASS,
                    2, nameof(BusMaster), "Failed to create relay task for CAN1->CAN2."
                );
                ESP_RETURN_ON_FALSE(
                    xTaskCreate(TestTask, "CAN2->CAN1", RELAY_TASK_STACK_SIZE, params2, RELAY_TASK_PRIORITY, &_can2TaskHandle) == pdPASS,
                    3, nameof(BusMaster), "Failed to create relay task for CAN2->CAN1."
                );
            }
            #endif

            return 0;
        }
    
    public:
        Test(Config::JsonFlash* config) : BusMaster(config) {}
    };
};
