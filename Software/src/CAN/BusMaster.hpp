#pragma once

#include <esp_check.h>
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include "Data/StaticConfig.h"
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
#include <map>
#include <vector>
#include <queue>
#include "EStringType.h"
#include "Samples.hpp"
#include <string>
#include "Data/PersistentData.hpp"
#include "Data/RuntimeStats.hpp"

// #define CAN_DUMP_BEFORE_INTERCEPT
#define CAN_DUMP_AFTER_INTERCEPT

namespace ReadieFur::OpenTCU::CAN
{
    class BusMaster : public Service::AService
    {
    private:
        static const TickType_t CAN_TIMEOUT_TICKS = pdMS_TO_TICKS(100);
        static const uint RELAY_TASK_STACK_SIZE = CONFIG_FREERTOS_IDLE_TASK_STACKSIZE + 1024;
        static const uint RELAY_TASK_PRIORITY = configMAX_PRIORITIES * 0.6;
        static const uint SECONDARY_TASK_STACK_SIZE = CONFIG_FREERTOS_IDLE_TASK_STACKSIZE + 1024;
        static const uint SECONDARY_TASK_PRIORITY = configMAX_PRIORITIES * 0.3;
        static const TickType_t SECONDARY_TASK_INTERVAL = pdMS_TO_TICKS(1000);
        #ifdef ENABLE_CAN_DUMP
        static const uint CAN_DUMP_QUEUE_SIZE = 500;
        #endif
        constexpr static const char* CONFIG_PATH = "/spiffs/can.json";

        struct SRelayTaskParameters
        {
            BusMaster* self;
            ACan* can1;
            ACan* can2;
        };

        #if SOC_TWAI_CONTROLLER_NUM <= 1
        spi_device_handle_t _mcpDeviceHandle = nullptr; //TODO: Move this to the MCP2515 file.
        #endif
        ACan* _can1 = nullptr;
        ACan* _can2 = nullptr;
        TaskHandle_t _can1TaskHandle = NULL;
        TaskHandle_t _can2TaskHandle = NULL;
        TaskHandle_t _secondaryTaskHandle = NULL;

        #pragma region Other data
        bool _savePersistentData = false;

        uint8_t _stringRequestType = 0;
        size_t _stringRequestBufferIndex = 0;
        char* _stringRequestBuffer = nullptr;
        // std::map<uint8_t, std::string> _strings;

        //TODO: Set an artificial speed limit with a lower wheel size and ease off the power as the limit is approached.
        double _wheelMultiplier = 1.0;
        #pragma endregion

        #pragma region Live data
        TickType_t _lastLiveDataUpdate = 0;

        Samples<uint16_t, uint32_t> _speedBuffer = Samples<uint16_t, uint32_t>(10);

        Samples<uint16_t, uint32_t> _batteryVoltage = Samples<uint16_t, uint32_t>(10);
        Samples<int32_t, int64_t> _batteryCurrent = Samples<int32_t, int64_t>(10);
        #pragma endregion

        #ifdef DEBUG
        size_t _sampleCount = 0;
        #endif

    protected:
        void SecondaryTask()
        {
            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                if (_savePersistentData)
                {
                    Data::PersistentData::Save();
                    _savePersistentData = false;
                }

                if (xTaskGetTickCount() - _lastLiveDataUpdate < pdMS_TO_TICKS(2000))
                {
                    Data::RuntimeStats::BikeSpeed = (uint32_t)(Data::RuntimeStats::RealSpeed / _wheelMultiplier);
                    Data::RuntimeStats::RealSpeed = _speedBuffer.Average();
                    // Data::RuntimeStats::Cadence = 0; //TODO: Implement cadence.
                    // Data::RuntimeStats::RiderPower = 0; //TODO: Implement rider power.
                    // Data::RuntimeStats::MotorPower = 0; //TODO: Implement motor power.
                    Data::RuntimeStats::BatteryVoltage = _batteryVoltage.Average();
                    Data::RuntimeStats::BatteryCurrent = _batteryCurrent.Average();
                }
                else
                {
                    //If the last live data update was over 5 seconds ago, consider the data to be broken/the bike is off.
                    Data::RuntimeStats::BikeSpeed =
                        Data::RuntimeStats::RealSpeed =
                        Data::RuntimeStats::Cadence =
                        Data::RuntimeStats::RiderPower =
                        Data::RuntimeStats::MotorPower =
                        Data::RuntimeStats::BatteryVoltage =
                        Data::RuntimeStats::BatteryCurrent =
                        Data::RuntimeStats::EaseSetting =
                        Data::RuntimeStats::PowerSetting = 0;
                    Data::RuntimeStats::WalkMode = false;
                }

                #ifdef DEBUG
                if (EnableRuntimeStats && xTaskGetTickCount() - _lastLiveDataUpdate < pdMS_TO_TICKS(2000))
                {
                    printf("Sample count: %i\n", _sampleCount);
                    _sampleCount = 0;

                    printf("Average bike speed: %u\n", Data::RuntimeStats::BikeSpeed);
                    printf("Average real speed: %u\n", Data::RuntimeStats::RealSpeed);

                    printf("Walk mode: %s\n", Data::RuntimeStats::WalkMode ? "On" : "Off");
                    printf("Ease setting: %u\n", Data::RuntimeStats::EaseSetting);
                    printf("Power setting: %u\n", Data::RuntimeStats::PowerSetting);

                    printf("Average battery voltage: %u\n", Data::RuntimeStats::BatteryVoltage);
                    printf("Average battery current: %li\n", Data::RuntimeStats::BatteryCurrent);
                }
                #endif

                vTaskDelay(SECONDARY_TASK_INTERVAL);
            }

            vTaskDelete(NULL);
        }

        virtual void RelayTask(void* param)
        {
            SRelayTaskParameters* params = static_cast<SRelayTaskParameters*>(param);

            char bus = pcTaskGetName(xTaskGetHandle(pcTaskGetName(NULL)))[3]; //Only really used for logging & debugging.
            char otherBus = bus == '1' ? '2' : '1';

            //Check if the task has been signalled for deletion.
            while (!ServiceCancellationToken.IsCancellationRequested())
            {
                //Attempt to read a message from the bus.
                SCanMessage message;
                esp_err_t res;
                if ((res = params->can1->Receive(&message, CAN_TIMEOUT_TICKS)) != ESP_OK)
                {
                    switch (res)
                    {
                    case ESP_ERR_TIMEOUT:
                        #if defined(DEBUG) && true
                        //While debugging I have the board externally powered so the bike can be off and this error is to be expected.
                        #else
                        //Messages should never time out as they are sent extremely frequently.
                        LOGW(nameof(CAN::BusMaster), "CAN%c timed out while waiting for message.", bus);
                        #endif
                        break;
                    case ESP_ERR_INVALID_STATE:
                        //TODO: Trigger a reset of the CAN controller.
                        LOGE(nameof(CAN::BusMaster), "CAN%c bus failure: %s", bus);
                        break;
                    default:
                        LOGE(nameof(CAN::BusMaster), "CAN%c failed to receive message: %i", bus, res);
                        break;
                    }
                    taskYIELD();
                    continue;
                }

                #if defined(ENABLE_CAN_DUMP) && defined(CAN_DUMP_BEFORE_INTERCEPT)
                LogMessage(bus, message);
                #endif



                //Analyze the message and modify it if needed.
                InterceptMessage(&message);

                #if defined(ENABLE_CAN_DUMP) && defined(CAN_DUMP_AFTER_INTERCEPT)
                LogMessage(bus, message);
                #endif

                //Relay the message to the other CAN bus.
                if ((res = params->can2->Send(message, CAN_TIMEOUT_TICKS)) != ESP_OK)
                {
                    switch (res)
                    {
                    case ESP_ERR_TIMEOUT:
                        //This should be an error as the message should always be sent successfully if the other bus is operational.
                        LOGE(nameof(CAN::BusMaster), "CAN%c timed out while waiting to relay message.", otherBus);
                        break;
                    case ESP_ERR_INVALID_STATE:
                        LOGE(nameof(CAN::BusMaster), "CAN%c bus failure: %s", otherBus);
                        break;
                    default:
                        LOGE(nameof(CAN::BusMaster), "CAN%c failed to relay message: %i", otherBus, res);
                        break;
                    }
                    taskYIELD();
                    continue;
                }

                //Yield to allow other higher priority tasks to run, but use this method over vTaskDelay(0) keep delay time to a minimal as this is a very high priority task.
                //We do not set a delay here as the delay is acted upon while waiting for CAN bus operations.
                taskYIELD();
            }

            vTaskDelete(NULL);
            delete params;
        }

        #ifdef ENABLE_CAN_DUMP
        inline virtual void LogMessage(char bus, SCanMessage& message)
        {
            //Copy the original message for logging.
            SCanDump dump =
            {
                .timestamp = esp_log_timestamp(),
                .bus = bus,
                .message = message //Creates a copy of the struct.
            };

            //Set wait time to 0 as this should not delay the task.
            #if defined(_LIVE_LOG)
            #elif false
            while (xQueueSend(BusMaster::CanDumpQueue, &dump, 0) == errQUEUE_FULL)
            {
                //If the queue is full, remove the oldest item.
                SCanDump oldDump;
                xQueueReceive(BusMaster::CanDumpQueue, &oldDump, 0);
            }
            #else
            if (xQueueSend(BusMaster::CanDumpQueue, &dump, 0) == errQUEUE_FULL)
                LOGW(nameof(CAN::BusMaster), "CAN log queue is full.");
            #endif
        }
        #endif

        //Force inline for minor performance improvements, ideal in this program as it will be called extremely frequently and is used for real-time data analysis.
        inline virtual void InterceptMessage(SCanMessage* message)
        {
            switch (message->id)
            {
            case 0x100:
            {
                if (message->data[0] == 0x05
                    && message->data[1] == 0x2E
                    && message->data[2] == 0x02
                    && message->data[3] == 0x06
                    && message->data[6] == 0x00
                    && message->data[7] == 0x00)
                {
                    LOGD(nameof(CAN::BusMaster), "Received request to set wheel circumference to: %u", message->data[4] | message->data[5] << 8);
                }
                break;
            }
            case 0x101:
            {
                if (message->data[0] == 0x10
                    && message->data[2] == 0x62
                    && message->data[3] == 0x02)
                {
                    LOGD(nameof(CAN::BusMaster), "Received string response for %x.", message->data[4]);
                    if (_stringRequestBuffer != nullptr)
                    {
                        LOGW(nameof(CAN::BusMaster), "New string request before previous request was completed, discarding previous request.");
                        delete[] _stringRequestBuffer;
                    }
                    _stringRequestType = message->data[4];
                    _stringRequestBuffer = new char[21]; //All string requests seem to be sent in a buffer of 20 bytes.
                    _stringRequestBufferIndex = 0;

                    //String response.
                    if (_stringRequestBuffer == nullptr)
                    {
                        LOGW(nameof(CAN::BusMaster), "String response received before a request was sent.");
                        break;
                    }

                    for (size_t i = 5; i < 8; i++)
                        _stringRequestBuffer[_stringRequestBufferIndex++] = (char)message->data[i];
                }
                else if (message->data[0] == 0x21 || message->data[0] == 0x22)
                {
                    //String response continued.
                    // LOGD(nameof(CAN::BusMaster), "Continued response for %x.", _stringRequestType);
                    if (_stringRequestBuffer == nullptr)
                    {
                        LOGW(nameof(CAN::BusMaster), "String response continued before a request was sent.");
                        break;
                    }

                    for (size_t i = 1; i < 8; i++)
                        _stringRequestBuffer[_stringRequestBufferIndex++] = (char)message->data[i];
                }
                else if (message->data[0] == 0x23)
                {
                    //String response end.
                    // LOGD(nameof(CAN::BusMaster), "String response end for %x.", _stringRequestType);
                    if (_stringRequestBuffer == nullptr)
                    {
                        LOGW(nameof(CAN::BusMaster), "String response end before a request was sent.");
                        break;
                    }

                    for (size_t i = 1; i < 4; i++)
                        _stringRequestBuffer[_stringRequestBufferIndex++] = (char)message->data[i];
                    _stringRequestBuffer[_stringRequestBufferIndex] = '\0';

                    LOGD(nameof(CAN::BusMaster), "String response for %x: %s", _stringRequestType, _stringRequestBuffer);


                    switch (_stringRequestType)
                    {
                    case EStringType::BikeSerialNumber:
                        Data::PersistentData::BikeSerialNumber = std::string(_stringRequestBuffer);
                        _savePersistentData = true;
                        break;
                    default:
                        break;
                    }
                    // _strings[_stringRequestType] = std::string(_stringRequestBuffer);
                    delete[] _stringRequestBuffer;
                    _stringRequestBuffer = nullptr;
                }
                else if (message->data[0] == 0x05
                    && message->data[1] == 0x62
                    && message->data[2] == 0x02
                    && message->data[3] == 0x06
                    && message->data[6] == 0xE0
                    && message->data[7] == 0xAA)
                {
                    uint16_t wheelCircumference = message->data[4] | message->data[5] << 8;
                    _wheelMultiplier = (double)Data::PersistentData::BaseWheelCircumference / Data::PersistentData::TargetWheelCircumference;
                    _savePersistentData = true;
                    LOGD(nameof(CAN::BusMaster), "Received wheel circumference: %u", wheelCircumference);
                    LOGD(nameof(CAN::BusMaster), "Wheel multiplier set to: %f", _wheelMultiplier);
                }
                break;
            }
            case 0x201:
            {
                //D1 and D2 combined contain the speed of the bike in km/h * 100 (little-endian).
                //Example: EA, 01 -> 01EA -> 490 -> 4.9km/h.
                //We won't work in decimals.
                uint16_t bikeSpeed = message->data[0] | message->data[1] << 8;
                uint16_t realSpeed = (uint16_t)(bikeSpeed * _wheelMultiplier);
                _speedBuffer.AddSample(realSpeed);
                message->data[0] = realSpeed & 0xFF;
                message->data[1] = realSpeed >> 8;
                _lastLiveDataUpdate = xTaskGetTickCount();
                break;
            }
            case 0x300:
            {
                //Assist settings.
                Data::RuntimeStats::WalkMode = message->data[1] == 0xA5;
                Data::RuntimeStats::EaseSetting = message->data[4];
                Data::RuntimeStats::PowerSetting = message->data[6];

                //If we are in walk mode and a speed multiplier exists, attempt to keep the walk speed at the original 5kph by setting the motor power to 0 when over a real speed of 5kph.
                if (Data::RuntimeStats::WalkMode && _wheelMultiplier != 1.0)
                {
                    uint16_t realSpeed = _speedBuffer.Latest();
                    if (realSpeed > 650) //Set to 650 to allow for some margin.
                    {
                        message->data[0] = 0; //Motor mode?
                        /*_easeSetting = */message->data[4] = 0;
                        /*_powerSetting = */message->data[6] = 0;
                    }
                }
                break;
            }
            case 0x401:
            {
                _batteryVoltage.AddSample(message->data[0] | message->data[1] << 8);

                //D5, D6, D7 and D8 combined contain the battery current in mA (little-endian with two's complement).
                //Example 1: 46, 00, 00, 00 -> 00 000046 -> 70mA.
                //Example 2: 5B, F0, FF, FF -> FF FFF05B -> -4005mA.
                _batteryCurrent.AddSample(message->data[4] | message->data[5] << 8 | message->data[6] << 16 | message->data[7] << 24);
                break;
            }
            default:
                break;
            }
        }

        void RunServiceImpl() override
        {
            esp_err_t err;

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
            if (_can1 == nullptr)
            {
                LOGE(nameof(CAN::BusMaster), "Failed to initialize CAN1.");
                return;
            }
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
            if (gpio_config(&mosiPinConfig) != ESP_OK
                || gpio_config(&misoPinConfig) != ESP_OK
                || gpio_config(&sckPinConfig) != ESP_OK
                || gpio_config(&csPinConfig) != ESP_OK
                || gpio_config(&intPinConfig) != ESP_OK)
            {
                LOGE(nameof(CAN::BusMaster), "Failed to initialize SPI bus: %i", 4);
                return;
            }

            spi_bus_config_t busConfig = {
                .mosi_io_num = SPI_MOSI_PIN,
                .miso_io_num = SPI_MISO_PIN,
                .sclk_io_num = SPI_SCK_PIN,
                .quadwp_io_num = -1,
                .quadhd_io_num = -1,
                .max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE,
            };
            //SPI2_HOST is the only SPI bus that can be used as GPSPI on the C3.
            if (spi_bus_initialize(SPI2_HOST, &busConfig, SPI_DMA_CH_AUTO) != ESP_OK)
            {
                LOGE(nameof(CAN::BusMaster), "Failed to initialize SPI bus: %i", 1);
                return;
            }

            spi_device_interface_config_t dev_config = {
                .mode = 0,
                .clock_speed_hz = SPI_MASTER_FREQ_8M, //Match the SPI CAN controller.
                .spics_io_num = SPI_CS_PIN,
                .queue_size = 2, //2 as per the specification: https://ww1.microchip.com/downloads/en/DeviceDoc/MCP2515-Stand-Alone-CAN-Controller-with-SPI-20001801J.pdf
            };
            if (spi_bus_add_device(SPI2_HOST, &dev_config, &_mcpDeviceHandle) != ESP_OK)
            {
                LOGE(nameof(CAN::BusMaster), "Failed to initialize SPI bus: %i", 1);
                return;
            }

            _can2 = new SpiCan(_spiDevice, CAN_250KBPS, MCP_8MHZ, SPI_INT_PIN);
            #endif
            if (_can2 == nullptr)
            {
                LOGE(nameof(CAN::BusMaster), "Failed to initialize CAN2: %i", 2);
                return;
            }
            #pragma endregion

            #ifdef ENABLE_CAN_DUMP
            CanDumpQueue = xQueueCreate(CAN_DUMP_QUEUE_SIZE, sizeof(BusMaster::SCanDump));
            if (CanDumpQueue == NULL)
            {
                LOGE(nameof(CAN::BusMaster), "Failed to create log queue.");
                return;
            }
            #endif

            #pragma region Tasks
            //Create high priority tasks to handle CAN relay tasks.
            //I am creating the parameters on the heap just in case this method returns before the task starts which will result in an error.

            //TODO: Determine if I should run both CAN tasks on one core and do secondary processing (i.e. metrics, user control, etc) on the other core, or split the load between all cores with CAN bus getting their own core.
            SRelayTaskParameters* params1 = new SRelayTaskParameters { this, _can1, _can2 };
            SRelayTaskParameters* params2 = new SRelayTaskParameters { this, _can2, _can1 };
            const TaskFunction_t relayTaskWrapper = [](void* param) { static_cast<SRelayTaskParameters*>(param)->self->RelayTask(param); };
            #if SOC_CPU_CORES_NUM > 1
            {
                if (xTaskCreatePinnedToCore(relayTaskWrapper, "CAN1->CAN2", RELAY_TASK_STACK_SIZE, params1, RELAY_TASK_PRIORITY, &_can1TaskHandle, 0) != pdPASS)
                {
                    LOGE(nameof(CAN::BusMaster), "Failed to create relay task for CAN1->CAN2.");
                    return;
                }
                if (xTaskCreatePinnedToCore(relayTaskWrapper, "CAN2->CAN1", RELAY_TASK_STACK_SIZE, params2, RELAY_TASK_PRIORITY, &_can2TaskHandle, 1) != pdPASS)
                {
                    LOGE(nameof(CAN::BusMaster), "Failed to create relay task for CAN2->CAN1.");
                    return;
                }
            }
            #else
            {
                if (xTaskCreate(relayTaskWrapper, "CAN1->CAN2", RELAY_TASK_STACK_SIZE, params1, RELAY_TASK_PRIORITY, &_can1TaskHandle) != pdPASS)
                {
                    LOGE(nameof(CAN::BusMaster), "Failed to create relay task for CAN1->CAN2.");
                    return;
                }
                if (xTaskCreate(relayTaskWrapper, "CAN2->CAN1", RELAY_TASK_STACK_SIZE, params2, RELAY_TASK_PRIORITY, &_can2TaskHandle) != pdPASS)
                {
                    LOGE(nameof(CAN::BusMaster), "Failed to create relay task for CAN2->CAN1.");
                    return;
                }
            }
            #endif
            #pragma endregion

            if (xTaskCreate([](void* param) { static_cast<BusMaster*>(param)->SecondaryTask(); }, "ConfigTask", SECONDARY_TASK_STACK_SIZE, this, SECONDARY_TASK_PRIORITY, &_secondaryTaskHandle) != pdPASS)
            {
                LOGE(nameof(CAN::BusMaster), "Failed to create config task.");
                return;
            }

            ServiceCancellationToken.WaitForCancellation();

            #pragma region Cleanup
            //Tasks should kill themselves when the cancellation token is triggered.
            delete _can1;
            _can1 = nullptr;
            delete _can2;
            _can2 = nullptr;

            _can1TaskHandle = nullptr;
            _can2TaskHandle = nullptr;
            _secondaryTaskHandle = nullptr;

            #if SOC_TWAI_CONTROLLER_NUM <= 1
            spi_bus_remove_device(_mcpDeviceHandle);
            spi_bus_free(SPI2_HOST);
            #endif
            #pragma endregion

            #ifdef ENABLE_CAN_DUMP
            vQueueDelete(CanDumpQueue);
            CanDumpQueue = NULL;
            #endif
        }

    public:
        #ifdef ENABLE_CAN_DUMP
        QueueHandle_t CanDumpQueue = NULL;
        struct SCanDump
        {
            ulong timestamp;
            char bus;
            SCanMessage message;
            //TODO: Add modified values.
        };
        #endif

        #ifdef DEBUG
        bool EnableRuntimeStats = true;
        #endif

        BusMaster()
        {
            ServiceEntrypointStackDepth += 1024;
            ServiceEntrypointPriority = RELAY_TASK_PRIORITY;
        }

        esp_err_t InjectMessage(bool bus, SCanMessage message)
        {
            LOGI(nameof(CAN::BusMaster), "Injecting message into CAN%c, ID: %x, Length: %i, Data: %02X %02X %02X %02X %02X %02X %02X %02X",
                bus ? '2' : '1',
                message.id,
                message.length,
                message.data[0],
                message.data[1],
                message.data[2],
                message.data[3],
                message.data[4],
                message.data[5],
                message.data[6],
                message.data[7]
            );

            if (bus)
                return _can2->Send(message, CAN_TIMEOUT_TICKS);
            else
                return _can1->Send(message, CAN_TIMEOUT_TICKS);

            return ESP_OK;
        }

        // std::string GetString(EStringType type)
        // {
        //     if (_strings.find(type) == _strings.end())
        //         return "";
        //     return _strings[type];
        // }

        esp_err_t SetTargetWheelCircumference(uint16_t circumference)
        {
            if (circumference > 2400 || circumference < 800)
            {
                LOGW(nameof(CAN::BusMaster), "Invalid wheel circumference: %u", circumference);
                return ESP_ERR_INVALID_ARG;
            }

            SCanMessage message =
            {
                .id = 0x100,
                .data = { 0x05, 0x2E, 0x02, 0x06, (uint8_t)(circumference & 0xFF), (uint8_t)(circumference >> 8), 0x00, 0x00 },
                .length = 8,
                .isExtended = false,
                .isRemote = false
            };
            esp_err_t err = InjectMessage(true, message);

            if (err != ESP_OK)
            {
                LOGE(nameof(CAN::BusMaster), "Failed to set wheel circumference: %i", err);
                return err;
            }

            Data::PersistentData::TargetWheelCircumference = circumference;
            return ESP_OK;
        }
    };
};
