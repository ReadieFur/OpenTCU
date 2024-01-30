#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <driver/spi_common.h>
#include <driver/twai.h>
#include "CAN/ACan.h"
#include "CAN/SpiCan.hpp"
#include "CAN/TwaiCan.hpp"
#include "Helpers.hpp"
#include "Debug.hpp"

#define CAN_TIMEOUT_TICKS pdMS_TO_TICKS(50)
#define RELAY_TASK_STACK_SIZE 1024 * 2.5
#define RELAY_TASK_PRIORITY configMAX_PRIORITIES - 10

class BusMaster
{
private:
    static bool initialized;
    static spi_device_handle_t* spiDevice;

    struct SRelayTaskParameters
    {
        ACan* canA;
        ACan* canB;
    };

    static void InterceptMessage(SCanMessage* message)
    {
        //TODO: Implement.
    }

    static void RelayTask(void* param)
    {
        SRelayTaskParameters* params = static_cast<SRelayTaskParameters*>(param);
        ACan* canA = params->canA;
        ACan* canB = params->canB;
        delete params;

        #ifdef ENABLE_CAN_DUMP
        bool isSPI = pcTaskGetTaskName(NULL)[0] == 'S';
        #endif

        while (true)
        {
            SCanMessage message;
            // TRACE("Waiting for message");
            if (esp_err_t receiveResult = canA->Receive(&message, CAN_TIMEOUT_TICKS) == ESP_OK)
            {
                InterceptMessage(&message);

                //Relay the message to the other CAN bus.
                esp_err_t sendResult = canB->Send(message, CAN_TIMEOUT_TICKS);
                if (sendResult != ESP_OK)
                {
                    TRACE("Failed to relay message: %x", sendResult);
                }
                else
                {
                    #ifdef VERY_VERBOSE
                    TRACE("Relayed message: %d, %d", message.id, message.length);
                    #endif
                }

                #ifdef ENABLE_CAN_DUMP
                Debug::SCanDump dump = {
                    .timestamp = xTaskGetTickCount(),
                    .isSPI = isSPI,
                    .message = message
                };
                if (BaseType_t queueResult = xQueueSend(Debug::canDumpQueue, &dump, 0) != pdTRUE)
                {
                    TRACE("Failed to add to dump queue: %d", queueResult);
                }
                #endif
            }
            else
            {
                #ifdef VERY_VERBOSE
                // TRACE("Timeout: %x", receiveResult);
                #endif
            }
        }
    }
public:
    static SpiCan* spiCAN;
    static TwaiCan* twaiCAN;

    static void Init()
    {
        if (initialized)
            return;
        initialized = true;

        #pragma region Setup SPI CAN
        //Configure the pins, all pins should be written low to start with.
        gpio_config_t mosiPinConfig = {
            .pin_bit_mask = 1ULL << SPI_MOSI_PIN,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config_t misoPinConfig = {
            .pin_bit_mask = 1ULL << SPI_MISO_PIN,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config_t sckPinConfig = {
            .pin_bit_mask = 1ULL << SPI_SCK_PIN,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config_t csPinConfig = {
            .pin_bit_mask = 1ULL << SPI_CS_PIN,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config_t intPinConfig = {
            .pin_bit_mask = 1ULL << SPI_INT_PIN,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE, //Use the internal pullup resistor as the trigger state of the MCP2515 is LOW.
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_NEGEDGE //Trigger on the falling edge.
        };
        ASSERT(gpio_config(&mosiPinConfig) == ESP_OK);
        ASSERT(gpio_config(&misoPinConfig) == ESP_OK);
        ASSERT(gpio_config(&sckPinConfig) == ESP_OK);
        ASSERT(gpio_config(&csPinConfig) == ESP_OK);
        ASSERT(gpio_config(&intPinConfig) == ESP_OK);

        spi_bus_config_t busConfig = {
            .mosi_io_num = SPI_MOSI_PIN,
            .miso_io_num = SPI_MISO_PIN,
            .sclk_io_num = SPI_SCK_PIN,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE,
        };
        //SPI2_HOST is the only SPI bus that can be used as GPSPI on the C3.
        ASSERT(spi_bus_initialize(SPI2_HOST, &busConfig, SPI_DMA_CH_AUTO) == ESP_OK);

        spi_device_interface_config_t dev_config = {
            .mode = 0,
            .clock_speed_hz = SPI_MASTER_FREQ_8M, //Match the SPI CAN controller.
            .spics_io_num = SPI_CS_PIN,
            .queue_size = 2, //2 as per the specification: https://ww1.microchip.com/downloads/en/DeviceDoc/MCP2515-Stand-Alone-CAN-Controller-with-SPI-20001801J.pdf
        };
        spiDevice = new spi_device_handle_t;
        ASSERT(spi_bus_add_device(SPI2_HOST, &dev_config, spiDevice) == ESP_OK);

        #ifdef CONFIG_COMPILER_CXX_EXCEPTIONS
        try { spiCAN = new SpiCan(*spiDevice, CAN_250KBPS, MCP_8MHZ, SPI_INT_PIN); }
        catch(const std::exception& e)
        {
            ERROR("%s", e.what());
            ASSERT(false);
        }
        #else
        spiCAN = new SpiCan(*spiDevice, CAN_250KBPS, MCP_8MHZ, SPI_INT_PIN);
        #endif
        #pragma endregion

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

        #ifdef CONFIG_COMPILER_CXX_EXCEPTIONS
        try
        {
            twaiCAN = new TwaiCan(
                TWAI_GENERAL_CONFIG_DEFAULT(
                    TWAI_TX_PIN,
                    TWAI_RX_PIN,
                    TWAI_MODE_NORMAL
                ),
                TWAI_TIMING_CONFIG_250KBITS(),
                TWAI_FILTER_CONFIG_ACCEPT_ALL()
            );
        }
        catch(const std::exception& e)
        {
            ERROR("%s", e.what());
            ASSERT(false);
        }
        #else
        twaiCAN = new TwaiCan(
            TWAI_GENERAL_CONFIG_DEFAULT(
                TWAI_TX_PIN,
                TWAI_RX_PIN,
                TWAI_MODE_NORMAL
            ),
            TWAI_TIMING_CONFIG_250KBITS(),
            TWAI_FILTER_CONFIG_ACCEPT_ALL()
        );
        #endif
        #pragma endregion

        #pragma region CAN task setup
        //Create high priority tasks to handle CAN relay tasks.
        //I am creating the parameters on the heap just incase this method returns before the task starts which will result in an error.
        xTaskCreate(RelayTask, "SPI->TWAI", RELAY_TASK_STACK_SIZE, new SRelayTaskParameters { spiCAN, twaiCAN }, RELAY_TASK_PRIORITY, NULL);
        xTaskCreate(RelayTask, "TWAI->SPI", RELAY_TASK_STACK_SIZE, new SRelayTaskParameters { twaiCAN, spiCAN }, RELAY_TASK_PRIORITY, NULL);
        #pragma endregion
    }

    static void Destroy()
    {
        if (!initialized)
            return;
        initialized = false;

        delete spiCAN;
        spiCAN = nullptr;

        delete twaiCAN;
        twaiCAN = nullptr;

        spi_bus_remove_device(*spiDevice);
        spi_bus_free(SPI2_HOST);
        delete spiDevice;
    }
};

bool BusMaster::initialized = false;
spi_device_handle_t* BusMaster::spiDevice = nullptr;
SpiCan* BusMaster::spiCAN = nullptr;
TwaiCan* BusMaster::twaiCAN = nullptr;
