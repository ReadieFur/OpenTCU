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

#define CAN_FREQUENCY 250000
#define CAN_INTERVAL_TICKS pdMS_TO_TICKS(1000 / CAN_FREQUENCY)
//Allow a max send timeout of half a frame.
#define CAN_SEND_TIMEOUT CAN_INTERVAL_TICKS / 2

#define RELAY_TASK_STACK_SIZE 1024
#define RELAY_TASK_PRIORITY configMAX_PRIORITIES - 2

SpiCan* can1;
TwaiCan* can2;

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
        //Wait indefinitely for a message to be received.
        if (canA->Receive(&message, portMAX_DELAY) == ESP_OK)
            //Relay the message to the other CAN bus.
            canB->Send(message, CAN_SEND_TIMEOUT);
    }
}

#ifdef DEBUG
void debug_setup(void* param)
{
    _debugServer = new AsyncWebServer(81);

    IPAddress ipAddress;
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    #ifdef GENERATOR
    IPAddress ip(192, 168, 0, 197);
    #else
    IPAddress ip(192, 168, 0, 199);
    #endif
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
        .queue_size = 6,
    };
    spi_device_handle_t spiDevice;
    assert(spi_bus_add_device(SPI2_HOST, &dev_config, &spiDevice) == ESP_OK);

    try { can1 = new SpiCan(spiDevice, CAN_250KBPS, MCP_8MHZ, CAN1_INT_PIN); }
    catch(const std::exception& e)
    {
        LOG(e.what());
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
        LOG(e.what());
        assert(false);
    }
    #pragma endregion

    #ifndef GENERATOR
    #pragma region CAN task setup
    //Create high priority tasks to handle CAN relay tasks.
    //I am creating the parameters on the heap just incase this method returns before the task starts which will result in an error.
    // xTaskCreate(RelayTask, "Can1RelayTask", RELAY_TASK_STACK_SIZE, new SRelayTaskParameters { can1, can2 }, RELAY_TASK_PRIORITY, NULL);
    // xTaskCreate(RelayTask, "Can2RelayTask", RELAY_TASK_STACK_SIZE, new SRelayTaskParameters { can2, can1 }, RELAY_TASK_PRIORITY, NULL);
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
    //Print CAN2 status.
    twai_status_info_t status;
    if (can2->GetStatus(status) == ESP_OK)
    {
        TRACE("Status: %d, %d, %d", status.state, status.tx_error_counter, status.rx_error_counter);
    }
    else
    {
        TRACE("Failed to get status.");
    }

    //Read from CAN2 (TWAI) and print to serial.
    //Use standard TWAI interface for testing instead of the ACan interface.
    twai_message_t message;
    esp_err_t res = twai_receive(&message, 5000 / portTICK_PERIOD_MS);
    if (res == ESP_OK)
    {
        TRACE("Received message: %d, %d, %d, %d, %d, %d, %d, %d, %d", message.identifier, message.flags, message.data_length_code, message.data[0], message.data[1], message.data[2], message.data[3], message.data[4], message.data[5]);
    }
    else
    {
        TRACE("Failed to receive message: %#x", res);
    }

    // vTaskDelete(NULL);
    #else
    // Debug::Blip();
    Logger::Log("");

    //Send a message every second.
    SCanMessage message = {
        .id = 0x123,
        .data = { i },
        .length = 1
    };
    can1->Send(message, portMAX_DELAY);
    Logger::Log("Sent message: %d, %d, %d", message.id, message.length, message.data[0]);

    //Receive the message that was sent and ensure that it is the same.
    SCanMessage receivedMessage;
    can2->Receive(&receivedMessage, portMAX_DELAY);
    if (receivedMessage.id != message.id || receivedMessage.length != message.length || receivedMessage.data[0] != message.data[0])
        Logger::Log("Message mismatch: %d, %d, %d", receivedMessage.id, receivedMessage.length, receivedMessage.data[0]);
    else
        Logger::Log("Message match: %d, %d, %d", receivedMessage.id, receivedMessage.length, receivedMessage.data[0]);

    i++;
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    #endif
}
