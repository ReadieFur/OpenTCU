#pragma once

#include "BusMaster.hpp"

namespace ReadieFur::OpenTCU::CAN
{
    class Test : public BusMaster
    {
    private:
        static const int TEST_CAN_TIMEOUT_TICKS = pdMS_TO_TICKS(500);

        void RelayTaskLocal(void* param)
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
                    LOGW("Test", "%i failed to send message.", id);
                    continue;
                }

                //Attempt to read a message from the bus.
                SCanMessage rxMessage;
                if (esp_err_t receiveResult = params->can2->Receive(&rxMessage, TEST_CAN_TIMEOUT_TICKS) != ESP_OK)
                {
                    LOGW("Test", "%i failed to receive message.", id);
                    continue;
                }

                //Print the received message.
                LOGI("Test", "%i received message: %i, %i", id, rxMessage.id, rxMessage.data[0]);

                taskYIELD();
            }

            delete params;
        }
    };
};
