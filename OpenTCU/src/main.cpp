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

#ifdef DEBUG
#include <esp_log.h>
#include <freertos/queue.h>

#define CAN_DUMP

#ifdef CAN_DUMP
#include <esp_task_wdt.h>

struct SCanDump
{
    uint32_t timestamp;
    bool isSPI;
    SCanMessage message;
};

QueueHandle_t _debugQueue = xQueueCreate(100, sizeof(SCanDump));

void CanDump(void* arg)
{
    // //Disable the watchdog timer as this method seems to cause the idle task to hang.
    // //This isn't ideal but won't be used for production so it'll make do for now.
    // //https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/wdts.html
    // esp_task_wdt_config_t config = {
    //     .timeout_ms = 0,
    //     .idle_core_mask = 0,
    //     .trigger_panic = false
    // };
    // esp_task_wdt_reconfigure(&config);
    // // esp_task_wdt_deinit();

    while (true)
    {
        SCanDump dump;
        if (xQueueReceive(_debugQueue, &dump, portMAX_DELAY) == pdTRUE)
        {
            // printf("[CAN] Direction: %s, id: %#x, isExtended: %d, isRemote: %d, length: %d, d0: %#x, d1: %#x, d2: %#x, d3: %#x, d4: %#x, d5: %#x, d6: %#x, d7: %#x\n",
            printf("[CAN]%d,%d,%x,%d,%d,%d,%x,%x,%x,%x,%x,%x,%x,%x\n",
                dump.timestamp,
                dump.isSPI,
                dump.message.id,
                dump.message.isExtended,
                dump.message.isRemote,
                dump.message.length,
                dump.message.data[0],
                dump.message.data[1],
                dump.message.data[2],
                dump.message.data[3],
                dump.message.data[4],
                dump.message.data[5],
                dump.message.data[6],
                dump.message.data[7]
            );
        }
    }
}
#endif

void Power(void* arg)
{
    vTaskDelay(5000 / portTICK_PERIOD_MS); //Wait 5 seconds before starting.
    while (true)
    {
        // TRACE("Power check pin: %d", gpio_get_level(POWER_CHECK_PIN));
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
    INFO("Debug setup started.");

    #ifdef CAN_DUMP
    xTaskCreate(CanDump, "CanDump", 4096, NULL, 1, NULL);
    #endif

    #if 1
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
    TRACE("CAN timeout set to %d ticks", CAN_TIMEOUT_TICKS);

    INFO("Debug setup finished.");
    vTaskDelete(NULL);
}
#endif

void RelayTask(void* param)
{
    SRelayTaskParameters* params = static_cast<SRelayTaskParameters*>(param);
    ACan* canA = params->canA;
    ACan* canB = params->canB;
    delete params;

    #ifdef CAN_DUMP
    bool isSPI = pcTaskGetTaskName(NULL)[0] == 'S';
    #endif

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
                // TRACE("Relayed message: %d, %d", message.id, message.length);
            }

            #ifdef CAN_DUMP
            SCanDump dump = {
                .timestamp = xTaskGetTickCount(),
                .isSPI = isSPI,
                .message = message
            };
            if (BaseType_t queueResult = xQueueSend(_debugQueue, &dump, 0) != pdTRUE)
            {
                TRACE("Failed to add to dump queue: %d", queueResult);
            }
            #endif
        }
        else
        {
            // TRACE("Timeout: %x", receiveResult);
        }
    }
}

void setup()
{
    //Used to indicate that the program has started.
    #ifdef DEBUG
    LED(0, 0, 1);
    #else
    gpio_config_t ledPinConfig = {
        .pin_bit_mask = 1ULL << LED_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    assert(gpio_config(&ledPinConfig) == ESP_OK);
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
    ASSERT(gpio_config(&mosiPinConfig) == ESP_OK);
    ASSERT(gpio_config(&misoPinConfig) == ESP_OK);
    ASSERT(gpio_config(&sckPinConfig) == ESP_OK);
    ASSERT(gpio_config(&csPinConfig) == ESP_OK);
    ASSERT(gpio_config(&intPinConfig) == ESP_OK);

    spi_bus_config_t busConfig = {
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = MISO_PIN,
        .sclk_io_num = SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE,
    };
    //SPI2_HOST is the only SPI bus that can be used as GPSPI on the C3.
    ASSERT(spi_bus_initialize(SPI2_HOST, &busConfig, SPI_DMA_CH_AUTO) == ESP_OK);

    spi_device_interface_config_t dev_config = {
        .mode = 0,
        .clock_speed_hz = SPI_MASTER_FREQ_8M, //Match the SPI CAN controller.
        .spics_io_num = CAN1_CS_PIN,
        .queue_size = 2, //2 as per the specification: https://ww1.microchip.com/downloads/en/DeviceDoc/MCP2515-Stand-Alone-CAN-Controller-with-SPI-20001801J.pdf
    };
    spi_device_handle_t spiDevice;
    ASSERT(spi_bus_add_device(SPI2_HOST, &dev_config, &spiDevice) == ESP_OK);

    #ifdef CONFIG_COMPILER_CXX_EXCEPTIONS
    try { spiCAN = new SpiCan(spiDevice, CAN_250KBPS, MCP_8MHZ, CAN1_INT_PIN); }
    catch(const std::exception& e)
    {
        ERROR("%s", e.what());
        ASSERT(false);
    }
    #else
    spiCAN = new SpiCan(spiDevice, CAN_250KBPS, MCP_8MHZ, CAN1_INT_PIN);
    #endif
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
    ASSERT(gpio_config(&txPinConfig) == ESP_OK);
    ASSERT(gpio_config(&rxPinConfig) == ESP_OK);

    #ifdef CONFIG_COMPILER_CXX_EXCEPTIONS
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
        ERROR("%s", e.what());
        ASSERT(false);
    }
    #else
    twaiCAN = new TwaiCan(
        TWAI_GENERAL_CONFIG_DEFAULT(
            CAN2_TX_PIN,
            CAN2_RX_PIN,
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

    //Signal that the program has setup.
    #ifdef DEBUG
    LED(0, 0, 0);
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
