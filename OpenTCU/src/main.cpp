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
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
AsyncWebServer* debugServer = nullptr;
#endif

#define ON LOW
#define OFF HIGH

ACan* can1;
ACan* can2;

void debug_setup(void* param)
{
    AsyncWebServer* _debugServer = (AsyncWebServer*)param;
    _debugServer = new AsyncWebServer(81);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        printf("WiFi Failed!\n");
        vTaskDelete(NULL);
    }
    WebSerial.begin(_debugServer);
    _debugServer->begin();
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    printf("Debug server started at %s\n", WiFi.localIP().toString().c_str());
    vTaskDelete(NULL);
}

void setup()
{
    //Used to indicate that the program has started.
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, ON);

    // #ifdef DEBUG
    xTaskCreate(debug_setup, "DebugSetup", 4096, debugServer, 1, NULL); //Low priority task as it is imperative that the CAN bus is setup first.
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

    try { can1 = new SpiCan(spiDevice, CAN_250KBPS, MCP_8MHZ); }
    catch(const std::exception& e) { assert(false); } //Cause the program to fail here.
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
    catch(const std::exception& e) { assert(false); }
    #pragma endregion

    //Signal that the program has setup.
    digitalWrite(LED_PIN, OFF);
}

void loop()
{
    vTaskDelete(NULL);
}
