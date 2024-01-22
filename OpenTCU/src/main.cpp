#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <driver/spi_common.h>
#include <driver/twai.h>
#include "CAN/ACan.hpp"
#include "CAN/SpiCan.hpp"
#include "CAN/TwaiCan.hpp"
#include <Arduino.h> //TODO: Remove this dependency.
#include "Helpers.h"
#ifdef DEBUG
#include <esp_log.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <WebSerialLite.h>
#endif

// #define CAN_FREQUENCY 250000 //Bits per second.
// #define BIT_SPEED 1.0 / CAN_FREQUENCY
// //Calculate the maximum number of bits that a message can contain.
// //https://en.wikipedia.org/wiki/CAN_bus
// #define FRAME_SIZE 1 + 29 + 1 + 1 + 18 + 1 + 2 + 4 + 64 + 15 + 1 + 1 + 1 + 7
// //Calculate the maximum amount of time it would take to send a message.
// #define MAX_MESSAGE_TIME FRAME_SIZE * BIT_SPEED
// //Add a multiplier of 2 to account for processing time.
// #define MAX_MESSAGE_TIME_MARGIN MAX_MESSAGE_TIME * 2
// //Convert the value from seconds to microseconds.
// #define MAX_MESSAGE_TIME_MARGIN_US MAX_MESSAGE_TIME_MARGIN * 1000000
// //Define the timeout in ticks.
// #define CAN_TIMEOUT_TICKS pdUS_TO_TICKS(MAX_MESSAGE_TIME_MARGIN_US)
// #define CAN_TIMEOUT_TICKS pdUS_TO_TICKS((((1 + 29 + 1 + 1 + 18 + 1 + 2 + 4 + 64 + 15 + 1 + 1 + 1 + 7) * (1.0 / 250000)) * 2) * 1000000)
//There was supposed to be some logic behind this value but I don't really know what I'm doing so I've just set an arbitrary value for now.
#define CAN_TIMEOUT_TICKS pdMS_TO_TICKS(10)

#define RELAY_TASK_STACK_SIZE 2048
#define RELAY_TASK_PRIORITY configMAX_PRIORITIES - 2

SpiCan* spiCAN;
TwaiCan* twaiCAN;

#if DEBUG
AsyncWebServer* _debugServer;
#endif

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
        // TRACE("Waiting for message");
        //Wait indefinitely for a message to be received.
        //TODO: Fix catching missed interrupts (can cause the program to hang). Two solutions both to be implemented are to add a timeout to the read operation and to fix the interrupt watchers themselves.
        if (canA->Receive(&message, portMAX_DELAY) == ESP_OK)
        {
            // TRACE("Got message");
            //Relay the message to the other CAN bus.
            canB->Send(message, CAN_TIMEOUT_TICKS);
            TRACE("Relayed message: %d, %d, %d", message.id, message.length, message.data[0]);
            // printf("\n");
        }
        else
        {
            // TRACE("Timeout");
        }
    }
}

#ifdef DEBUG
void debug_setup(void* param)
{
    TRACE("Debug setup started.");

    #if 1
    _debugServer = new AsyncWebServer(81);

    IPAddress ipAddress;
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    IPAddress ip(192, 168, 0, 199);
    IPAddress subnet(255, 255, 254, 0);
    IPAddress gateway(192, 168, 1, 254);
    WiFi.config(ip, gateway, subnet);
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        TRACE("STA Failed!");

        //Create AP instead.
        WiFi.mode(WIFI_AP);
        //Create AP using mac address.
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char ssid[18];
        sprintf(ssid, "ESP32-%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        WiFi.softAP(ssid);
        ipAddress = WiFi.softAPIP();
    }
    else
    {
        ipAddress = WiFi.localIP();
    }

    WebSerial.begin(_debugServer);
    _debugServer->begin();

    TRACE("Debug server started at %s", ipAddress.toString().c_str());
    #endif

    //Delayed logs.
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    // TRACE("CAN timeout %fus", MAX_MESSAGE_TIME_MARGIN_US);
    TRACE("CAN timeout set to %d ticks", CAN_TIMEOUT_TICKS);

    TRACE("Debug setup finished.");
    vTaskDelete(NULL);
}
#endif

void setup()
{
    //Used to indicate that the program has started.
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, ON);

    #ifdef DEBUG
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    xTaskCreate(debug_setup, "DebugSetup", 4096, NULL, 1, NULL); //Low priority task as it is imperative that the CAN bus is setup first.
    #endif

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
        .queue_size = 7,
    };
    spi_device_handle_t spiDevice;
    assert(spi_bus_add_device(SPI2_HOST, &dev_config, &spiDevice) == ESP_OK);

    try { spiCAN = new SpiCan(spiDevice, CAN_250KBPS, MCP_8MHZ, CAN1_INT_PIN); }
    catch(const std::exception& e)
    {
        LOG(e.what());
        assert(false);
    }
    #pragma endregion

    #pragma region Setup TWAI CAN
    try
    {
        twaiCAN = new TwaiCan(
            TWAI_GENERAL_CONFIG_DEFAULT(
                CAN2_TX_PIN,
                CAN2_RX_PIN,
                TWAI_MODE_NORMAL
            ),
            TWAI_TIMING_CONFIG_250KBITS(),
            TWAI_FILTER_CONFIG_ACCEPT_ALL()
        );
    }
    catch(const std::exception& e)
    {
        LOG(e.what());
        assert(false);
    }
    #pragma endregion

    #pragma region CAN task setup
    //Create high priority tasks to handle CAN relay tasks.
    //I am creating the parameters on the heap just incase this method returns before the task starts which will result in an error.
    xTaskCreate(RelayTask, "Can1RelayTask", RELAY_TASK_STACK_SIZE, new SRelayTaskParameters { spiCAN, twaiCAN }, RELAY_TASK_PRIORITY, NULL);
    xTaskCreate(RelayTask, "Can2RelayTask", RELAY_TASK_STACK_SIZE, new SRelayTaskParameters { twaiCAN, spiCAN }, RELAY_TASK_PRIORITY, NULL);
    #pragma endregion

    //Signal that the program has setup.
    digitalWrite(LED_PIN, OFF);
}

void loop()
{
    vTaskDelete(NULL);
}
