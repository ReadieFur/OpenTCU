#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <driver/spi_common.h>
#include <driver/twai.h>
#include "CAN/ACan.h"
#include "CAN/SpiCan.hpp"
#include "CAN/TwaiCan.hpp"
#include "Helpers.h"
#ifdef DEBUG
#include <esp_log.h>
#include <SmartLeds.h>
#endif

#define CAN_TIMEOUT_TICKS pdMS_TO_TICKS(50)
#define RELAY_TASK_STACK_SIZE 1024 * 2.5
#define RELAY_TASK_PRIORITY configMAX_PRIORITIES - 10

SpiCan* spiCAN;
TwaiCan* twaiCAN;

struct SRelayTaskParameters
{
    ACan* canA;
    ACan* canB;
};

void RelayTask(void* param)
{
    SRelayTaskParameters* params = static_cast<SRelayTaskParameters*>(param);
    ACan* canA = params->canA;
    ACan* canB = params->canB;
    delete params;
    while (true)
    {
        SCanMessage message;
        // TRACE("Waiting for message");
        if (esp_err_t receiveResult = canA->Receive(&message, CAN_TIMEOUT_TICKS) == ESP_OK)
        {
            // TRACE("Got message");
            //Relay the message to the other CAN bus.
            esp_err_t sendResult = canB->Send(message, CAN_TIMEOUT_TICKS);
            if (sendResult != ESP_OK)
            {
                TRACE("Failed to relay message: %x", sendResult);
            }
            else
            {
                TRACE("Relayed message: %d, %d", message.id, message.length);
            }
        }
        else
        {
            // TRACE("Timeout: %x", receiveResult);
        }
    }
}

#ifdef DEBUG
SmartLed led(LED_WS2812B, 1, LED_PIN, 0, DoubleBuffer);

void SetLed(uint8_t r, uint8_t g, uint8_t b)
{
    led[0] = Rgb{r, g, b};
    led.show();
    // led.wait();
}

void Power(void* arg)
{
    vTaskDelay(5000 / portTICK_PERIOD_MS); //Wait 5 seconds before starting.
    while (true)
    {
        TRACE("Power check pin: %d", gpio_get_level(POWER_CHECK_PIN));
        if (gpio_get_level(POWER_CHECK_PIN) == 0)
        {
            TRACE("Powering on");
            gpio_set_level(POWER_PIN, 1);
            vTaskDelay(250 / portTICK_PERIOD_MS);
            gpio_set_level(POWER_PIN, 0);
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void DebugSetup(void* param)
{
    TRACE("Debug setup started.");

    #if 0
    gpio_config_t powerPinConfig = {
        .pin_bit_mask = 1ULL << POWER_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config_t powerCheckPinConfig = {
        .pin_bit_mask = 1ULL << POWER_CHECK_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    assert(gpio_config(&powerPinConfig) == ESP_OK);
    assert(gpio_config(&powerCheckPinConfig) == ESP_OK);
    xTaskCreate(Power, "Power", 2048, NULL, 1, NULL);
    #endif

    #if 0
    _debugServer = new AsyncWebServer(81);

    IPAddress ipAddress;
    WiFi.mode(WIFI_STA);
    // IPAddress ip(192, 168, 0, 158);
    // IPAddress subnet(255, 255, 254, 0);
    // IPAddress gateway(192, 168, 1, 254);
    // WiFi.config(ip, gateway, subnet);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    //https://github.com/esphome/issues/issues/4893
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
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
    gpio_config_t ledPinConfig = {
        .pin_bit_mask = 1ULL << LED_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    assert(gpio_config(&ledPinConfig) == ESP_OK);
    #ifdef DEBUG
    SetLed(0, 0, 255);
    #else
    gpio_set_level(LED_PIN, 1);
    #endif

    #ifdef DEBUG
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    xTaskCreate(DebugSetup, "DebugSetup", 4096, NULL, 1, NULL); //Low priority task as it is imperative that the CAN bus is setup first.
    #endif

    #pragma region Setup SPI CAN
    //Configure the pins, all pins should be written low to start with.
    gpio_config_t mosiPinConfig = {
        .pin_bit_mask = 1ULL << MOSI_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config_t misoPinConfig = {
        .pin_bit_mask = 1ULL << MISO_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config_t sckPinConfig = {
        .pin_bit_mask = 1ULL << SCK_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config_t csPinConfig = {
        .pin_bit_mask = 1ULL << CAN1_CS_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config_t intPinConfig = {
        .pin_bit_mask = 1ULL << CAN1_INT_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, //Use the internal pullup resistor as the trigger state of the MCP2515 is LOW.
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE //Trigger on the falling edge.
        // .intr_type = GPIO_INTR_LOW_LEVEL
    };
    assert(gpio_config(&mosiPinConfig) == ESP_OK);
    assert(gpio_config(&misoPinConfig) == ESP_OK);
    assert(gpio_config(&sckPinConfig) == ESP_OK);
    assert(gpio_config(&csPinConfig) == ESP_OK);
    assert(gpio_config(&intPinConfig) == ESP_OK);

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
        .queue_size = 2, //2 as per the specification: https://ww1.microchip.com/downloads/en/DeviceDoc/MCP2515-Stand-Alone-CAN-Controller-with-SPI-20001801J.pdf
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
    //Configure GPIO.
    gpio_config_t txPinConfig = {
        .pin_bit_mask = 1ULL << CAN2_TX_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config_t rxPinConfig = {
        .pin_bit_mask = 1ULL << CAN2_RX_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    assert(gpio_config(&txPinConfig) == ESP_OK);
    assert(gpio_config(&rxPinConfig) == ESP_OK);

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
    xTaskCreate(RelayTask, "SPI->TWAI", RELAY_TASK_STACK_SIZE, new SRelayTaskParameters { spiCAN, twaiCAN }, RELAY_TASK_PRIORITY, NULL);
    xTaskCreate(RelayTask, "TWAI->SPI", RELAY_TASK_STACK_SIZE, new SRelayTaskParameters { twaiCAN, spiCAN }, RELAY_TASK_PRIORITY, NULL);
    #pragma endregion

    //Signal that the program has setup.
    #ifdef DEBUG
    // SetLed(0, 0, 0);
    #else
    gpio_set_level(LED_PIN, 0);
    #endif

    //app_main IS allowed to return according to the ESP32 and FreeRTOS documentation.
}

void loop()
{
    vTaskDelete(NULL);
}

#ifndef ARDUINO
extern "C" void app_main()
{
    setup();
    // while (true)
    //     loop();
}
#endif
