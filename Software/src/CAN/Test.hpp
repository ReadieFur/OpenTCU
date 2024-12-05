#pragma once

#include "BusMaster.hpp"

namespace ReadieFur::OpenTCU::CAN
{
    class Test : public BusMaster
    {
    private:
        static const uint TEST_CAN_DELAY_MIN = 500;
        static const uint TEST_CAN_DELAY_MAX = 1500;
        static const TickType_t TEST_CAN_TIMEOUT_TICKS = pdMS_TO_TICKS(500);

        TickType_t GetRandomDelay(uint min, uint max)
        {
            return pdMS_TO_TICKS(min + (rand() % (max - min)));
        }

        void RelayTaskLocal(void* param) override
        {
            if constexpr (sizeof(TickType_t) > sizeof(SCanMessage::data))
            {
                // #error "Timestamp size is too large for the data field."
                LOGE(nameof(Test), "Timestamp size is too large for the data field.");
                abort();
            }

            SRelayTaskParameters* params = static_cast<SRelayTaskParameters*>(param);

            TaskHandle_t taskHandle = xTaskGetHandle(pcTaskGetName(NULL));
            uint32_t id = pcTaskGetName(taskHandle)[3] == '1' ? 0x1 : 0x2;

            if (id == 0x1)
                vTaskDelay(GetRandomDelay(250, 750)); //Random delay to stagger the start of the tasks.

            //Check if the task has been signalled for deletion.
            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                //Generate a message to send.
                SCanMessage txMessage =
                {
                    .id = id,
                    .length = sizeof(TickType_t) //Sizeof gives the number of bytes needed to store the timestamp, we can send a maximum of 8 bytes.
                };
                TickType_t timestamp = xTaskGetTickCount();
                for (int i = 0; i < sizeof(TickType_t); i++)
                {
                    //i*8 is the number of bits to shift the timestamp to the right to get the byte at position i.
                    //&0xFF is a mask to get only the lower 8 bits of the shifted timestamp.
                    txMessage.data[i] = (timestamp >> (i * 8)) & 0xFF;
                }

                //Attempt to send a message to the bus.
                LOGI(nameof(Test), "%i sending message: %i, %i", id, txMessage.id, timestamp);
                if (params->can1->Send(txMessage, TEST_CAN_TIMEOUT_TICKS) != ESP_OK)
                {
                    LOGW(nameof(Test), "%i failed to send message.", id);
                    taskYIELD();
                    continue;
                }

                //Attempt to read a message from the bus.
                SCanMessage rxMessage;
                if (esp_err_t receiveResult = params->can2->Receive(&rxMessage, TEST_CAN_TIMEOUT_TICKS) != ESP_OK)
                {
                    LOGW(nameof(Test), "%i failed to receive message.", id);
                    taskYIELD();
                    continue;
                }

                //Reconstruct the timestamp from the received message.
                TickType_t receivedTimestamp = 0;
                for (int i = 0; i < sizeof(TickType_t); i++) //Reconstruct the timestamp from the received message, should be the same size as the sent timestamp.
                {
                    //i*8 is the number of bits to shift the timestamp to the left to get the byte at position i.
                    //| is a bitwise OR to combine the bytes into the timestamp.
                    receivedTimestamp |= rxMessage.data[i] << (i * 8);
                }

                //Validate the received message.
                if (rxMessage.id != id)
                {
                    LOGW(nameof(Test), "%i received message with incorrect ID: %i", id, rxMessage.id);
                    taskYIELD();
                    continue;
                }
                else if (receivedTimestamp != timestamp)
                {
                    LOGW(nameof(Test), "%i received message with incorrect timestamp: %i", id, receivedTimestamp);
                    taskYIELD();
                    continue;
                }
                
                //Print the received message.
                LOGI(nameof(Test), "%i valid message received: %i, %i", id, rxMessage.id, receivedTimestamp);

                // taskYIELD();
                vTaskDelay(GetRandomDelay(TEST_CAN_DELAY_MIN, TEST_CAN_DELAY_MAX)); //Random delay to stagger the messages.
            }

            vTaskDelete(NULL);
            delete params;
        }

    public:
        Test() : BusMaster() {}
    };
};
