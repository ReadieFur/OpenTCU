#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <driver/spi_common.h>
#include <driver/twai.h>
#include "ACan.h"
#include "SpiCan.hpp"
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
    static spi_device_handle_t _spiDevice;

    struct SRelayTaskParameters
    {
        ACan* canA;
        ACan* canB;
    };

    static esp_err_t InterceptMessage(SCanMessage* message)
    {
        // TRACE("Got message");
        //TODO: Implement.

        return ESP_OK;
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
                switch (esp_err_t interceptResult = InterceptMessage(&message))
                {
                case ESP_OK:
                case ESP_ERR_NOT_SUPPORTED:
                    break;
                default:
                    LOG_ERROR("Failed to process message '%x': %x", message.id, interceptResult)
                    continue;
                }

                //Relay the message to the other CAN bus.
                esp_err_t sendResult = canB->Send(message, CAN_TIMEOUT_TICKS);
                if (sendResult != ESP_OK)
                {
                    LOG_ERROR("Failed to relay message: %x", sendResult);
                }
                // else
                // {
                //     LOG_TRACE("Relayed message: %d, %d", message.id, message.length);
                // }

                #ifdef ENABLE_CAN_DUMP
                SCanDump dump = {
                    .isSPI = isSPI,
                    .message = message
                };

                struct tm timeInfo;
                if (getLocalTime(&timeInfo))
                {
                    //Get Epoch time.
                    time(&dump.timestamp);
                }
                else
                {
                    dump.timestamp = xTaskGetTickCount();
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
        ASSERT(spi_bus_add_device(SPI2_HOST, &dev_config, &_spiDevice) == ESP_OK);

        spiCan = new SpiCan(_spiDevice, CAN_250KBPS, MCP_8MHZ, SPI_INT_PIN);
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
            #ifdef CHIP_S3
            xTaskCreatePinnedToCore(
            #else
            xTaskCreate(
            #endif
                RelayTask, "SPI->TWAI", RELAY_TASK_STACK_SIZE, new SRelayTaskParameters { spiCan, twaiCan }, RELAY_TASK_PRIORITY, &spiToTwaiTaskHandle
            #ifdef CHIP_S3
                , 0
            #endif
            ) == pdPASS);

        ASSERT(
            #ifdef CHIP_S3
            xTaskCreatePinnedToCore(
            #else
            xTaskCreate(
            #endif
                RelayTask, "TWAI->SPI", RELAY_TASK_STACK_SIZE, new SRelayTaskParameters { twaiCan, spiCan }, RELAY_TASK_PRIORITY, &twaiToSpiTaskHandle
            #if defined(CHIP_S3) && (!defined(DEBUG) && true) //When debugging do all CAN bus work on core x and run everything else on core y.
                , 1
            #elif defined(CHIP_S3)
                , 0
            #endif
            ) == pdPASS);
        #pragma endregion
    }

    static void Destroy()
    {
        if (_running)
            Stop();

        delete spiCan;
        spiCan = nullptr;

        delete twaiCan;
        twaiCan = nullptr;

        spi_bus_remove_device(_spiDevice);
        spi_bus_free(SPI2_HOST);
    }

public:
    static SpiCan* spiCan;
    static TwaiCan* twaiCan;
    static TaskHandle_t spiToTwaiTaskHandle;
    static TaskHandle_t twaiToSpiTaskHandle;
    #ifdef ENABLE_CAN_DUMP
    static QueueHandle_t canDumpQueue;
    struct SCanDump
    {
        long timestamp;
        bool isSPI;
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

        vTaskDelete(spiToTwaiTaskHandle);
        vTaskDelete(twaiToSpiTaskHandle);

        _running = false;
    }
};

bool BusMaster::_running = false;
spi_device_handle_t BusMaster::_spiDevice;
SpiCan* BusMaster::spiCan = nullptr;
TwaiCan* BusMaster::twaiCan = nullptr;
TaskHandle_t BusMaster::spiToTwaiTaskHandle;
TaskHandle_t BusMaster::twaiToSpiTaskHandle;
#ifdef ENABLE_CAN_DUMP
QueueHandle_t BusMaster::canDumpQueue = xQueueCreate(100, sizeof(BusMaster::SCanDump));
#endif