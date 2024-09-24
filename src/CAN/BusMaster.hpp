#pragma once

#include <Arduino.h>
#include "pch.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <driver/spi_common.h>
#include <driver/twai.h>
#include "Abstractions/AService.hpp"
#include "ACan.h"
#include "McpCan.hpp"
#include "TwaiCan.hpp"
#include "Config/JsonFlash.hpp"

namespace ReadieFur::OpenTCU::CAN
{
    class BusMaster : public Abstractions::AService
    {
    private:
        static const int CAN_TIMEOUT_TICKS = pdMS_TO_TICKS(50);
        static const int RELAY_TASK_STACK_SIZE = configIDLE_TASK_STACK_SIZE * 2.5;
        static const int RELAY_TASK_PRIORITY = configMAX_PRIORITIES * 0.6;
        static const int CONFIG_TASK_STACK_SIZE = configIDLE_TASK_STACK_SIZE * 2.5;
        static const int CONFIG_TASK_PRIORITY = configMAX_PRIORITIES * 0.3;
        static const int CONFIG_TASK_INTERVAL = pdMS_TO_TICKS(1000);

        struct SRelayTaskParameters
        {
            BusMaster* self;
            ACan* canA;
            ACan* canB;
        };

        Config::JsonFlash* _config;
        spi_device_handle_t _mcpDeviceHandle = nullptr;
        McpCan* _mcpCan = nullptr;
        TwaiCan* _twaiCan = nullptr;
        TaskHandle_t _configTaskHandle = NULL;
        TaskHandle_t _mcpToTwaiTaskHandle = NULL;
        TaskHandle_t _twaiToMcpTaskHandle = NULL;

        static void RelayTask(void* param)
        {
            SRelayTaskParameters* params = static_cast<SRelayTaskParameters*>(param);

            //Check if the task has been signalled for deletion.
            while (eTaskGetState(NULL) != eTaskState::eDeleted)
            {
                //Attempt to read a message from the bus.
                SCanMessage message;
                if (esp_err_t receiveResult = params->canA->Receive(&message, CAN_TIMEOUT_TICKS) != ESP_OK)
                    continue;

                //Analyze the message and modify it if needed.
                params->self->InterceptMessage(&message);

                //Relay the message to the other CAN bus.
                params->canB->Send(message, CAN_TIMEOUT_TICKS);

                //Yield to allow other higher priority tasks to run, but use this method over vTaskDelay(0) keep delay time to a minimal as this is a very high priority task.
                taskYIELD();
            }

            delete params;
        }

        static void ReadConfigTask(void* param)
        {
            BusMaster* self = static_cast<BusMaster*>(param);

            while (eTaskGetState(NULL) != eTaskState::eDeleted)
            {
                //TODO: Implement.
                vTaskDelay(CONFIG_TASK_INTERVAL);
            }
        }

        void InterceptMessage(SCanMessage* message)
        {
            //TODO: Implement.
        }

    protected:
        int InstallServiceImpl() override
        {
            #pragma region Setup MCP CAN
            //Configure the pins, all pins should be written low to start with (pulldown).
            pinMode(MCP_MOSI_PIN, OUTPUT);
            pinMode(MCP_MISO_PIN, INPUT_PULLDOWN);
            pinMode(MCP_SCK_PIN, OUTPUT);
            pinMode(MCP_CS_PIN, OUTPUT);
            pinMode(MCP_INT_PIN, INPUT_PULLDOWN);

            spi_bus_config_t busConfig = {
                .mosi_io_num = MCP_MOSI_PIN,
                .miso_io_num = MCP_MISO_PIN,
                .sclk_io_num = MCP_SCK_PIN,
                .quadwp_io_num = -1,
                .quadhd_io_num = -1,
                .max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE,
            };
            //SPI2_HOST is the only SPI bus that can be used as GPSPI on the C3.
            ESP_RETURN_ON_FALSE(spi_bus_initialize(SPI2_HOST, &busConfig, SPI_DMA_CH_AUTO) == ESP_OK, 1, nameof(BusMaster), "Failed to initialize SPI bus: %i", 1);

            spi_device_interface_config_t dev_config = {
                .mode = 0,
                .clock_speed_hz = SPI_MASTER_FREQ_8M, //Match the SPI CAN controller.
                .spics_io_num = MCP_CS_PIN,
                .queue_size = 2, //2 as per the specification: https://ww1.microchip.com/downloads/en/DeviceDoc/MCP2515-Stand-Alone-CAN-Controller-with-SPI-20001801J.pdf
            };
            ESP_RETURN_ON_FALSE(spi_bus_add_device(SPI2_HOST, &dev_config, &_mcpDeviceHandle) == ESP_OK, 1, nameof(BusMaster), "Failed to initialize SPI bus: %i", 1);

            _mcpCan = McpCan::Initialize(_mcpDeviceHandle, CAN_250KBPS, MCP_8MHZ, MCP_INT_PIN);
            ESP_RETURN_ON_FALSE(_mcpCan != nullptr, 2, nameof(BusMaster), "Failed to initialize MCP device: %i", 2);
            #pragma endregion

            #pragma region Setup TWAI CAN
            //Configure GPIO.
            pinMode(TWAI_TX_PIN, OUTPUT);
            pinMode(TWAI_RX_PIN, INPUT_PULLDOWN);

            _twaiCan = TwaiCan::Initialize(
                TWAI_GENERAL_CONFIG_DEFAULT(
                    TWAI_TX_PIN,
                    TWAI_RX_PIN,
                    TWAI_MODE_NORMAL
                ),
                TWAI_TIMING_CONFIG_250KBITS(),
                TWAI_FILTER_CONFIG_ACCEPT_ALL()
            );
            ESP_RETURN_ON_FALSE(_twaiCan != nullptr, 3, nameof(BusMaster), "Failed to initialize TWAI device: %i", 3);
            #pragma endregion

            return 0;
        }

        int UninstallServiceImpl() override
        {
            delete _mcpCan;
            _mcpCan = nullptr;

            delete _twaiCan;
            _twaiCan = nullptr;

            spi_bus_remove_device(_mcpDeviceHandle);
            spi_bus_free(SPI2_HOST);

            return 0;
        }

        int StartServiceImpl() override
        {
            //Create high priority tasks to handle CAN relay tasks.
            //I am creating the parameters on the heap just in case this method returns before the task starts which will result in an error.

            //TODO: Determine if I should run both CAN tasks on one core and do secondary processing (i.e. metrics, user control, etc) on the other core, or split the load between all cores with CAN bus getting their own core.
            SRelayTaskParameters* mcpToTwaiParams = new SRelayTaskParameters { this, _mcpCan, _twaiCan };
            SRelayTaskParameters* twaiToMcpParams = new SRelayTaskParameters { this, _twaiCan, _mcpCan };
            if constexpr (SOC_CPU_CORES_NUM > 1)
            {
                ESP_RETURN_ON_FALSE(
                    xTaskCreate(ReadConfigTask, "CANReadCfg", CONFIG_TASK_STACK_SIZE, this, CONFIG_TASK_PRIORITY, &_configTaskHandle),
                    1, nameof(BusMaster), "Failed to create config watcher task."
                );
                ESP_RETURN_ON_FALSE(
                    xTaskCreatePinnedToCore(RelayTask, "MCP->TWAI", RELAY_TASK_STACK_SIZE, mcpToTwaiParams, RELAY_TASK_PRIORITY, &_mcpToTwaiTaskHandle, 0) == pdPASS,
                    2, nameof(BusMaster), "Failed to create relay task for MCP->TWAI."
                );
                ESP_RETURN_ON_FALSE(
                    xTaskCreatePinnedToCore(RelayTask, "TWAI->MCP", RELAY_TASK_STACK_SIZE, twaiToMcpParams, RELAY_TASK_PRIORITY, &_twaiToMcpTaskHandle, 1) == pdPASS,
                    3, nameof(BusMaster), "Failed to create relay task for TWAI->MCP."
                );
            }
            else
            {
                ESP_RETURN_ON_FALSE(
                    xTaskCreate(ReadConfigTask, "CANReadCfg", CONFIG_TASK_STACK_SIZE, this, CONFIG_TASK_PRIORITY, &_configTaskHandle),
                    1, nameof(BusMaster), "Failed to create config watcher task."
                );
                ESP_RETURN_ON_FALSE(
                    xTaskCreate(RelayTask, "MCP->TWAI", RELAY_TASK_STACK_SIZE, mcpToTwaiParams, RELAY_TASK_PRIORITY, &_mcpToTwaiTaskHandle) == pdPASS,
                    2, nameof(BusMaster), "Failed to create relay task for MCP->TWAI."
                );
                ESP_RETURN_ON_FALSE(
                    xTaskCreate(RelayTask, "TWAI->MCP", RELAY_TASK_STACK_SIZE, twaiToMcpParams, RELAY_TASK_PRIORITY, &_twaiToMcpTaskHandle) == pdPASS,
                    3, nameof(BusMaster), "Failed to create relay task for TWAI->MCP."
                );
            }

            return 0;
        }

        int StopServiceImpl() override
        {
            vTaskDelete(_configTaskHandle);
            vTaskDelete(_mcpToTwaiTaskHandle);
            vTaskDelete(_twaiToMcpTaskHandle);
            return 0;
        }

    public:
        #ifdef ENABLE_CAN_DUMP
        QueueHandle_t canDumpQueue = xQueueCreate(100, sizeof(BusMaster::SCanDump));
        struct SCanDump
        {
            long timestamp;
            bool isSPI;
            SCanMessage message;
        };
        #endif

        BusMaster(Config::JsonFlash* config) : _config(config) {}
    };
};
