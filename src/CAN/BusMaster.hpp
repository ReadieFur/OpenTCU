#pragma once

#include <Arduino.h>
#include "Common.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <driver/spi_common.h>
#include <driver/twai.h>
#include "AService.hpp"
#include "ACan.h"
#include "McpCan.hpp"
#include "TwaiCan.hpp"

class BusMaster : public AService
{
private:
    static const int CAN_TIMEOUT_TICKS = pdMS_TO_TICKS(50);
    static const int RELAY_TASK_STACK_SIZE = 1024 * 2.5;
    static const int RELAY_TASK_PRIORITY = configMAX_PRIORITIES - 10;

    spi_device_handle_t _mcpDeviceHandle;

    struct SRelayTaskParameters
    {
        ACan* canA;
        ACan* canB;
    };

    static void RelayTask(void* param)
    {
        SRelayTaskParameters* params = static_cast<SRelayTaskParameters*>(param);
        ACan* canA = params->canA;
        ACan* canB = params->canB;
        delete params;

        while (true)
        {
            SCanMessage message;
            if (esp_err_t receiveResult = canA->Receive(&message, CAN_TIMEOUT_TICKS) != ESP_OK)
                continue;

            //Relay the message to the other CAN bus.
            canB->Send(message, CAN_TIMEOUT_TICKS);
        }
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
        ESP_RETURN_ON_FALSE(spi_bus_initialize(SPI2_HOST, &busConfig, SPI_DMA_CH_AUTO) == ESP_OK, 1, nameof(BusMaster), "Failed to initialize SPI bus.");

        spi_device_interface_config_t dev_config = {
            .mode = 0,
            .clock_speed_hz = SPI_MASTER_FREQ_8M, //Match the SPI CAN controller.
            .spics_io_num = MCP_CS_PIN,
            .queue_size = 2, //2 as per the specification: https://ww1.microchip.com/downloads/en/DeviceDoc/MCP2515-Stand-Alone-CAN-Controller-with-SPI-20001801J.pdf
        };
        ESP_RETURN_ON_FALSE(spi_bus_add_device(SPI2_HOST, &dev_config, &_mcpDeviceHandle) == ESP_OK, 1, nameof(BusMaster), "Failed to initialize SPI bus.");

        mcpCan = new McpCan(_mcpDeviceHandle, CAN_250KBPS, MCP_8MHZ, MCP_INT_PIN);
        #pragma endregion

        #pragma region Setup TWAI CAN
        //Configure GPIO.
        pinMode(TWAI_TX_PIN, OUTPUT);
        pinMode(TWAI_RX_PIN, INPUT_PULLDOWN);

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

        return 0;
    }

    int UninstallServiceImpl() override
    {
        delete mcpCan;
        mcpCan = nullptr;

        delete twaiCan;
        twaiCan = nullptr;

        spi_bus_remove_device(_mcpDeviceHandle);
        spi_bus_free(SPI2_HOST);

        return 0;
    }

    int StartServiceImpl() override
    {
        //Create high priority tasks to handle CAN relay tasks.
        //I am creating the parameters on the heap just in case this method returns before the task starts which will result in an error.

        //TODO: Determine if I should run both CAN tasks on one core and do secondary processing (i.e. metrics, user control, etc) on the other core, or split the load between all cores with CAN bus getting their own core.
        if constexpr (SOC_CPU_CORES_NUM > 1)
        {
            xTaskCreatePinnedToCore(RelayTask, "MCP->TWAI", RELAY_TASK_STACK_SIZE, new SRelayTaskParameters { mcpCan, twaiCan }, RELAY_TASK_PRIORITY, &mcpToTwaiTaskHandle, 0);
            xTaskCreatePinnedToCore(RelayTask, "TWAI->MCP", RELAY_TASK_STACK_SIZE, new SRelayTaskParameters { twaiCan, mcpCan }, RELAY_TASK_PRIORITY, &twaiToMcpTaskHandle, 1);
        }
        else
        {
            xTaskCreate(RelayTask, "MCP->TWAI", RELAY_TASK_STACK_SIZE, new SRelayTaskParameters { mcpCan, twaiCan }, RELAY_TASK_PRIORITY, &mcpToTwaiTaskHandle);
            xTaskCreate(RelayTask, "TWAI->MCP", RELAY_TASK_STACK_SIZE, new SRelayTaskParameters { twaiCan, mcpCan }, RELAY_TASK_PRIORITY, &twaiToMcpTaskHandle);
        }

        return 0;
    }

    int StopServiceImpl() override
    {
        vTaskDelete(mcpToTwaiTaskHandle);
        vTaskDelete(twaiToMcpTaskHandle);

        return 0;
    }

public:
    McpCan* mcpCan = nullptr;
    TwaiCan* twaiCan = nullptr;
    TaskHandle_t mcpToTwaiTaskHandle;
    TaskHandle_t twaiToMcpTaskHandle;
    #ifdef ENABLE_CAN_DUMP
    QueueHandle_t canDumpQueue = xQueueCreate(100, sizeof(BusMaster::SCanDump));
    struct SCanDump
    {
        long timestamp;
        bool isSPI;
        SCanMessage message;
    };
    #endif
};
