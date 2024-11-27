#pragma once

#include "pch.h"
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <driver/spi_common.h>
#include <driver/twai.h>
#include <Service/AService.hpp>
#include "ACan.h"
#if SOC_TWAI_CONTROLLER_NUM <= 1
#include "McpCan.hpp"
#endif
#include "TwaiCan.hpp"
#include "Logging.hpp"

namespace ReadieFur::OpenTCU::CAN
{
    class BusMaster : public Service::AService
    {
    protected:
        static const TickType_t CAN_TIMEOUT_TICKS = pdMS_TO_TICKS(50);
        static const uint RELAY_TASK_STACK_SIZE = CONFIG_FREERTOS_IDLE_TASK_STACKSIZE * 2.5;
        static const uint RELAY_TASK_PRIORITY = configMAX_PRIORITIES * 0.6;
        static const uint CONFIG_TASK_STACK_SIZE = CONFIG_FREERTOS_IDLE_TASK_STACKSIZE * 2.5;
        static const uint CONFIG_TASK_PRIORITY = configMAX_PRIORITIES * 0.3;
        static const TickType_t CONFIG_TASK_INTERVAL = pdMS_TO_TICKS(1000);

        struct SRelayTaskParameters
        {
            BusMaster* self;
            ACan* can1;
            ACan* can2;
        };

    private:
        #if SOC_TWAI_CONTROLLER_NUM <= 1
        spi_device_handle_t _mcpDeviceHandle = nullptr; //TODO: Move this to the MCP2515 file.
        #endif
        ACan* _can1 = nullptr;
        ACan* _can2 = nullptr;
        TaskHandle_t _configTaskHandle = NULL;
        TaskHandle_t _can1TaskHandle = NULL;
        TaskHandle_t _can2TaskHandle = NULL;

        static void RelayTask(void* param)
        {
            static_cast<SRelayTaskParameters*>(param)->self->RelayTaskLocal(param);
        }

    protected:
        virtual void RelayTaskLocal(void* param)
        {
            SRelayTaskParameters* params = static_cast<SRelayTaskParameters*>(param);

            bool isCan1 = pcTaskGetName(xTaskGetHandle(pcTaskGetName(NULL)))[3] == '1';

            //Check if the task has been signalled for deletion.
            while (!params->self->ServiceCancellationToken.IsCancellationRequested())
            {
                //Attempt to read a message from the bus.
                SCanMessage message;
                esp_err_t res = ESP_OK;
                if ((res = params->can1->Receive(&message, CAN_TIMEOUT_TICKS)) != ESP_OK)
                {
                    LOGW(nameof(BusMaster), "CAN%i failed to receive message: %i", isCan1 ? 1 : 2, res);
                    taskYIELD();
                    continue;
                }

                //Analyze the message and modify it if needed.
                params->self->InterceptMessage(&message);

                //Relay the message to the other CAN bus.
                res = ESP_OK;
                if ((res = params->can2->Send(message, CAN_TIMEOUT_TICKS)) != ESP_OK)
                {
                    LOGW(nameof(BusMaster), "CAN%i failed to relay message: %i", isCan1 ? 1 : 2, res);
                    taskYIELD();
                    continue;
                }

                //Yield to allow other higher priority tasks to run, but use this method over vTaskDelay(0) keep delay time to a minimal as this is a very high priority task.
                taskYIELD();
            }

            vTaskDelete(NULL);
            delete params;
        }

        //Force inline for minor performance improvements, ideal in this program as it will be called extremely frequently and is used for real-time data analysis.
        inline virtual void InterceptMessage(SCanMessage* message)
        {
            //TODO: Implement.
        }

        void RunServiceImpl() override
        {
            #pragma region CAN1
            gpio_config_t hostTxPinConfig1 = {
                .pin_bit_mask = 1ULL << TWAI1_RX_PIN, //Have the GPIO config inverted, e.g. the RX of the CAN controller is the TX of the host.
                .mode = GPIO_MODE_OUTPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_ENABLE,
                .intr_type = GPIO_INTR_DISABLE
            };
            gpio_config_t hostRxPinConfig1 = {
                .pin_bit_mask = 1ULL << TWAI1_TX_PIN,
                .mode = GPIO_MODE_INPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_ENABLE,
                .intr_type = GPIO_INTR_DISABLE
            };
            ESP_ERROR_CHECK(gpio_config(&hostTxPinConfig1));
            ESP_ERROR_CHECK(gpio_config(&hostRxPinConfig1));

            _can1 = TwaiCan::Initialize(
                TWAI_GENERAL_CONFIG_DEFAULT_V2(
                    0,
                    TWAI1_TX_PIN, //It seems this driver wants the pinout of the controller, not the host, i.e. pass the controller TX pin to the TX parameter, instead of the host TX pin (which would be the RX pin of the controller).
                    TWAI1_RX_PIN,
                    TWAI_MODE_NORMAL
                ),
                TWAI_TIMING_CONFIG_250KBITS(),
                TWAI_FILTER_CONFIG_ACCEPT_ALL()
            );
            ESP_RETURN_ON_FALSE(_can1 != nullptr,, nameof(BusMaster), "Failed to initialize CAN1: %i", 3);
            #pragma endregion

            #pragma region CAN2
            #if SOC_TWAI_CONTROLLER_NUM > 1
            gpio_config_t hostTxPinConfig2 = {
                .pin_bit_mask = 1ULL << TWAI2_RX_PIN,
                .mode = GPIO_MODE_OUTPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_ENABLE,
                .intr_type = GPIO_INTR_DISABLE
            };
            gpio_config_t hostRxPinConfig2 = {
                .pin_bit_mask = 1ULL << TWAI2_TX_PIN,
                .mode = GPIO_MODE_INPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_ENABLE,
                .intr_type = GPIO_INTR_DISABLE
            };
            ESP_ERROR_CHECK(gpio_config(&hostTxPinConfig2));
            ESP_ERROR_CHECK(gpio_config(&hostRxPinConfig2));

            _can2 = TwaiCan::Initialize(
                TWAI_GENERAL_CONFIG_DEFAULT_V2(
                    1,
                    TWAI2_TX_PIN,
                    TWAI2_RX_PIN,
                    TWAI_MODE_NORMAL
                ),
                TWAI_TIMING_CONFIG_250KBITS(),
                TWAI_FILTER_CONFIG_ACCEPT_ALL()
            );
            #else
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
            ESP_RETURN_ON_FALSE(gpio_config(&mosiPinConfig) == ESP_OK,, nameof(BusMaster), "Failed to initialize SPI bus: %i", 4);
            ESP_RETURN_ON_FALSE(gpio_config(&misoPinConfig) == ESP_OK,, nameof(BusMaster), "Failed to initialize SPI bus: %i", 4);
            ESP_RETURN_ON_FALSE(gpio_config(&sckPinConfig) == ESP_OK,, nameof(BusMaster), "Failed to initialize SPI bus: %i", 4);
            ESP_RETURN_ON_FALSE(gpio_config(&csPinConfig) == ESP_OK,, nameof(BusMaster), "Failed to initialize SPI bus: %i", 4);
            ESP_RETURN_ON_FALSE(gpio_config(&intPinConfig) == ESP_OK,, nameof(BusMaster), "Failed to initialize SPI bus: %i", 4);

            spi_bus_config_t busConfig = {
                .mosi_io_num = SPI_MOSI_PIN,
                .miso_io_num = SPI_MISO_PIN,
                .sclk_io_num = SPI_SCK_PIN,
                .quadwp_io_num = -1,
                .quadhd_io_num = -1,
                .max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE,
            };
            //SPI2_HOST is the only SPI bus that can be used as GPSPI on the C3.
            ESP_RETURN_ON_FALSE(spi_bus_initialize(SPI2_HOST, &busConfig, SPI_DMA_CH_AUTO) == ESP_OK,, nameof(BusMaster), "Failed to initialize SPI bus: %i", 1);

            spi_device_interface_config_t dev_config = {
                .mode = 0,
                .clock_speed_hz = SPI_MASTER_FREQ_8M, //Match the SPI CAN controller.
                .spics_io_num = SPI_CS_PIN,
                .queue_size = 2, //2 as per the specification: https://ww1.microchip.com/downloads/en/DeviceDoc/MCP2515-Stand-Alone-CAN-Controller-with-SPI-20001801J.pdf
            };
            ESP_RETURN_ON_FALSE(spi_bus_add_device(SPI2_HOST, &dev_config, &_mcpDeviceHandle) == ESP_OK,, nameof(BusMaster), "Failed to initialize SPI bus: %i", 1);

            _can2 = new SpiCan(_spiDevice, CAN_250KBPS, MCP_8MHZ, SPI_INT_PIN);
            #endif
            ESP_RETURN_ON_FALSE(_can2 != nullptr,, nameof(BusMaster), "Failed to initialize CAN2: %i", 2);
            #pragma endregion

            #pragma region Tasks
            //Create high priority tasks to handle CAN relay tasks.
            //I am creating the parameters on the heap just in case this method returns before the task starts which will result in an error.

            //TODO: Determine if I should run both CAN tasks on one core and do secondary processing (i.e. metrics, user control, etc) on the other core, or split the load between all cores with CAN bus getting their own core.
            SRelayTaskParameters* params1 = new SRelayTaskParameters { this, _can1, _can2 };
            SRelayTaskParameters* params2 = new SRelayTaskParameters { this, _can2, _can1 };

            #if SOC_CPU_CORES_NUM > 1
            {
                ESP_RETURN_ON_FALSE(
                    xTaskCreatePinnedToCore(RelayTask, "CAN1->CAN2", RELAY_TASK_STACK_SIZE, params1, RELAY_TASK_PRIORITY, &_can1TaskHandle, 0) == pdPASS,
                    , nameof(BusMaster), "Failed to create relay task for CAN1->CAN2."
                );
                ESP_RETURN_ON_FALSE(
                    xTaskCreatePinnedToCore(RelayTask, "CAN2->CAN1", RELAY_TASK_STACK_SIZE, params2, RELAY_TASK_PRIORITY, &_can2TaskHandle, 1) == pdPASS,
                    , nameof(BusMaster), "Failed to create relay task for CAN2->CAN1."
                );
            }
            #else
            {
                ESP_RETURN_ON_FALSE(
                    xTaskCreate(RelayTask, "CAN1->CAN2", RELAY_TASK_STACK_SIZE, params1, RELAY_TASK_PRIORITY, &_can1TaskHandle) == pdPASS,
                    , nameof(BusMaster), "Failed to create relay task for CAN1->CAN2."
                );
                ESP_RETURN_ON_FALSE(
                    xTaskCreate(RelayTask, "CAN2->CAN1", RELAY_TASK_STACK_SIZE, params2, RELAY_TASK_PRIORITY, &_can2TaskHandle) == pdPASS,
                    , nameof(BusMaster), "Failed to create relay task for CAN2->CAN1."
                );
            }
            #endif
            #pragma endregion

            ServiceCancellationToken.WaitForCancellation();

            #pragma region Cleanup
            //Tasks should kill themselves when the cancellation token is triggered.
            delete _can1;
            _can1 = nullptr;
            delete _can2;
            _can2 = nullptr;

            #if SOC_TWAI_CONTROLLER_NUM <= 1
            spi_bus_remove_device(_mcpDeviceHandle);
            spi_bus_free(SPI2_HOST);
            #endif
            #pragma endregion
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

        BusMaster()
        {
            ServiceEntrypointStackDepth += 1024;
        }
    };
};
