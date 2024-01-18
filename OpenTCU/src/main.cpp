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

ACan* can1;
ACan* can2;

#define ON LOW
#define OFF HIGH

void setup()
{
    #ifdef DEBUG
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, ON);
    #endif

    Logger::Begin();

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

    #if DEBUG
    digitalWrite(LED_PIN, OFF);
    #endif
}

void loop()
{
    // vTaskDelete(NULL);
    printf("Looping...");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
