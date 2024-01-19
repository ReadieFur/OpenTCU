#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <driver/spi_common.h>
#include <driver/twai.h>
#include "Logger.hpp"
#include "CAN/ACan.hpp"
#include "CAN/SpiCan.hpp"
#include "CAN/TwaiCan.hpp"
#include <Arduino.h> //TODO: Remove this dependency.
#ifdef DEBUG
#include "Debug.hpp"
#endif

#define CAN_FREQUENCY 250000
#define CAN_INTERVAL_TICKS pdMS_TO_TICKS(1000 / CAN_FREQUENCY)
//Allow a max send timeout of half a frame.
#define CAN_SEND_TIMEOUT CAN_INTERVAL_TICKS / 2

#define RELAY_TASK_STACK_SIZE 1024
#define RELAY_TASK_PRIORITY configMAX_PRIORITIES - 2

ACan* can1;
ACan* can2;

struct SRelayTaskParameters
{
    ACan* canA;
    ACan* canB;
};

//TODO: Add concurrency protection (devices cannot read and write at the same time).
void RelayTask(void* param)
{
    SRelayTaskParameters* params = static_cast<SRelayTaskParameters*>(param);
    ACan* canA = params->canA;
    ACan* canB = params->canB;
    delete params;
    while(true)
    {
        SCanMessage message;
        //Wait indefinitely for a message to be received.
        if (canA->Receive(&message, portMAX_DELAY) == ESP_OK)
            //Relay the message to the other CAN bus.
            canB->Send(message, CAN_SEND_TIMEOUT);
    }
}

void debug_setup(void* param)
{
    Debug::Setup();
    vTaskDelete(NULL);
}

void setup()
{
    //Used to indicate that the program has started.
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, ON);

    // #ifdef DEBUG
    xTaskCreate(debug_setup, "DebugSetup", 4096, NULL, 1, NULL); //Low priority task as it is imperative that the CAN bus is setup first.
    // #endif

    #pragma region Setup SPI CAN
    spi_bus_config_t bus_config = {
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = MISO_PIN,
        .sclk_io_num = SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE,
    };
    //SPI2_HOST is the only SPI bus that can be used as GPSPI on the C3.
    assert(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO) == ESP_OK);

    spi_device_interface_config_t dev_config = {
        .mode = 0,
        .clock_speed_hz = SPI_MASTER_FREQ_8M, //Match the SPI CAN controller.
        .spics_io_num = CAN1_CS_PIN,
        .queue_size = 6,
    };
    spi_device_handle_t spiDevice;
    assert(spi_bus_add_device(SPI2_HOST, &dev_config, &spiDevice) == ESP_OK);

    try { can1 = new SpiCan(spiDevice, CAN_250KBPS, MCP_8MHZ, CAN1_INT_PIN); }
    catch(const std::exception& e)
    {
        // TRACE(e.what());
        Logger::Log(e.what());
        assert(false);
    }
    #pragma endregion

    #pragma region Setup TWAI CAN
    try
    {
        can2 = new TwaiCan(
            TWAI_GENERAL_CONFIG_DEFAULT(CAN2_TX_PIN, CAN2_RX_PIN, TWAI_MODE_NORMAL),
            TWAI_TIMING_CONFIG_250KBITS(),
            TWAI_FILTER_CONFIG_ACCEPT_ALL()
        );
    }
    catch(const std::exception& e)
    {
        // TRACE(e.what());
        Logger::Log(e.what());
        assert(false);
    }
    #pragma endregion

    #ifndef GENERATOR
    #pragma region CAN task setup
    //Create high priority tasks to handle CAN relay tasks.
    //I am creating the parameters on the heap just incase this method returns before the task starts which will result in an error.
    xTaskCreate(RelayTask, "Can1RelayTask", RELAY_TASK_STACK_SIZE, new SRelayTaskParameters { can1, can2 }, RELAY_TASK_PRIORITY, NULL);
    xTaskCreate(RelayTask, "Can2RelayTask", RELAY_TASK_STACK_SIZE, new SRelayTaskParameters { can2, can1 }, RELAY_TASK_PRIORITY, NULL);
    #pragma endregion
    #endif

    //Signal that the program has setup.
    digitalWrite(LED_PIN, OFF);
}

#ifdef GENERATOR
uint8_t i = 0;
#endif

void loop()
{
    #ifndef GENERATOR
    vTaskDelete(NULL);
    #else
    // Debug::Blip();
    Logger::Log("\n");

    //Send a message every second.
    SCanMessage message = {
        .id = 0x123,
        .data = { i },
        .length = 1
    };
    can1->Send(message, portMAX_DELAY);
    Logger::Log("Sent message: %d, %d, %d\n", message.id, message.length, message.data[0]);

    //Receive the message that was sent and ensure that it is the same.
    SCanMessage receivedMessage;
    can2->Receive(&receivedMessage, portMAX_DELAY);
    if (receivedMessage.id != message.id || receivedMessage.length != message.length || receivedMessage.data[0] != message.data[0])
        Logger::Log("Message mismatch: %d, %d, %d\n", receivedMessage.id, receivedMessage.length, receivedMessage.data[0]);
    else
        Logger::Log("Message match: %d, %d, %d\n", receivedMessage.id, receivedMessage.length, receivedMessage.data[0]);

    i++;
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    #endif
}

#if 0
void loop(void* param)
{
    while(true)
        loop();
}

extern "C" void app_main()
{
    setup();
    xTaskCreatePinnedToCore(loop, "Loop", 8096, NULL, 1, NULL, 1);
}
#endif
