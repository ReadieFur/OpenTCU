#include <freertos/FreeRTOS.h>
#include <Helpers.h>
#include <unity.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <driver/spi_common.h>
#include <driver/twai.h>
#include <esp_random.h>
#include <CAN/ACan.h>
#include <CAN/SpiCan.hpp>
#include <CAN/TwaiCan.hpp>

#define CAN_TIMEOUT_TICKS pdMS_TO_TICKS(50)
//Ideally I would want the delay to be 0 but it is unpredictable how long it will take for the CAN controllers to process the data, it seems like the upper limit of the random delay is 2ms.
#define TEST_DELAY() vTaskDelay(pdMS_TO_TICKS(2))
// #define TEST_DELAY()

spi_device_handle_t spiDevice;
SpiCan* spiCAN;
TwaiCan* twaiCAN;

#ifndef MANUAL_CONFIGURATION
void setUp(void)
#else
void setup()
#endif
{
    esp_log_level_set("*", ESP_LOG_VERBOSE);

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

    spi_bus_config_t busConfig = {
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = MISO_PIN,
        .sclk_io_num = SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE,
    };
    //SPI2_HOST is the only SPI bus that can be used as GPSPI on the C3.
    assert(spi_bus_initialize(SPI2_HOST, &busConfig, SPI_DMA_CH_AUTO) == ESP_OK);

    spi_device_interface_config_t dev_config = {
        .mode = 0,
        .clock_speed_hz = SPI_MASTER_FREQ_8M, //Match the SPI CAN controller.
        .spics_io_num = CAN1_CS_PIN,
        .queue_size = 2, //2 as per the specification: https://ww1.microchip.com/downloads/en/DeviceDoc/MCP2515-Stand-Alone-CAN-Controller-with-SPI-20001801J.pdf
    };
    assert(spi_bus_add_device(SPI2_HOST, &dev_config, &spiDevice) == ESP_OK);

    spiCAN = new SpiCan(spiDevice, CAN_250KBPS, MCP_8MHZ, CAN1_INT_PIN);
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

    twaiCAN = new TwaiCan(
        TWAI_GENERAL_CONFIG_DEFAULT(
            CAN2_TX_PIN,
            CAN2_RX_PIN,
            TWAI_MODE_NORMAL
        ),
        TWAI_TIMING_CONFIG_250KBITS(),
        TWAI_FILTER_CONFIG_ACCEPT_ALL()
    );
    #pragma endregion
}

#ifndef MANUAL_CONFIGURATION
void tearDown(void)
#else
void teardown()
#endif
{
    delete spiCAN;
    spi_bus_remove_device(spiDevice);
    spi_bus_free(SPI2_HOST);

    delete twaiCAN;
}

uint8_t UInt8Random()
{
    uint8_t smallRandom;
    esp_fill_random(&smallRandom, sizeof(smallRandom));
    return smallRandom;
}

void TwoWayTest(const char* tag, ACan* canA, ACan* canB)
{
    #pragma region CAN A
    // uint32_t statusA1;
    // TEST_ASSERT_EQUAL(ESP_OK, canA->GetStatus(&statusA1, CAN_TIMEOUT_TICKS));
    // INFO("%s: canA status 1: %d", tag, statusA1);

    SCanMessage canATxMessage;
    canATxMessage.id = UInt8Random();
    canATxMessage.length = 1;
    canATxMessage.data[0] = UInt8Random();
    //These INFO calls seem to spit out random values so I am using the data inside of the CAN classes instead and displaying those with TRACE.
    // INFO("%s: canATxMessage.id: %x, canATxMessage.length: %d, canATxMessage.data[0]: %x", tag, canATxMessage.id, canATxMessage.length, canATxMessage.data[0]);
    TEST_ASSERT_EQUAL(ESP_OK, canA->Send(canATxMessage, CAN_TIMEOUT_TICKS));

    // uint32_t statusA2;
    // TEST_ASSERT_EQUAL(ESP_OK, canA->GetStatus(&statusA2, CAN_TIMEOUT_TICKS));
    // INFO("%s: canA status 2: %d", tag, statusA2);
    #pragma endregion

    TEST_DELAY();

    #pragma region CAN B
    // uint32_t statusB1;
    // TEST_ASSERT_EQUAL(ESP_OK, canB->GetStatus(&statusB1, CAN_TIMEOUT_TICKS));
    // INFO("%s: canB status 1: %d", tag, statusB1);

    SCanMessage canBRxMessage;
    // INFO("%s: canBRxMessage.id: %x, canBRxMessage.length: %d, canBRxMessage.data[0]: %x", tag, canBRxMessage.id, canBRxMessage.length, canBRxMessage.data[0]);
    TEST_ASSERT_EQUAL(ESP_OK, canB->Receive(&canBRxMessage, CAN_TIMEOUT_TICKS));

    // uint32_t statusB2;
    // TEST_ASSERT_EQUAL(ESP_OK, canB->GetStatus(&statusB2, CAN_TIMEOUT_TICKS));
    // INFO("%s: canB status 2: %d", tag, statusB2);
    #pragma endregion

    //Check if the message is the same as the one sent.
    TEST_ASSERT_EQUAL(canATxMessage.id, canBRxMessage.id);
    TEST_ASSERT_EQUAL(canATxMessage.length, canBRxMessage.length);
    TEST_ASSERT_EQUAL(canATxMessage.data[0], canBRxMessage.data[0]);
}

void FourWayTest(const char* tag, ACan* canA, ACan* canB)
{
    INFO("%s: FourWayTest", tag);

    SCanMessage canATxMessage;
    canATxMessage.id = UInt8Random();
    canATxMessage.length = 1;
    canATxMessage.data[0] = UInt8Random();
    // INFO("canATxMessage.id: %x, canATxMessage.length: %d, canATxMessage.data[0]: %x", canATxMessage.id, canATxMessage.length, canATxMessage.data[0]);
    TEST_ASSERT_EQUAL(ESP_OK, canA->Send(canATxMessage, CAN_TIMEOUT_TICKS));

    TEST_DELAY();

    SCanMessage canBMessage;
    // INFO("canBMessage.id: %x, canBMessage.length: %d, canBMessage.data[0]: %x", canBMessage.id, canBMessage.length, canBMessage.data[0]);
    TEST_ASSERT_EQUAL(ESP_OK, canB->Receive(&canBMessage, CAN_TIMEOUT_TICKS));

    //Check if the message is the same as the one sent.
    TEST_ASSERT_EQUAL(canATxMessage.id, canBMessage.id);
    TEST_ASSERT_EQUAL(canATxMessage.length, canBMessage.length);
    TEST_ASSERT_EQUAL(canATxMessage.data[0], canBMessage.data[0]);

    // INFO("canBTxMessage.id: %x, canBTxMessage.length: %d, canBTxMessage.data[0]: %x", canBMessage.id, canBMessage.length, canBMessage.data[0]);
    TEST_ASSERT_EQUAL(ESP_OK, canB->Send(canBMessage, CAN_TIMEOUT_TICKS));

    TEST_DELAY();

    SCanMessage canARxMessage;
    // INFO("canARxMessage.id: %x, canARxMessage.length: %d, canARxMessage.data[0]: %x", canARxMessage.id, canARxMessage.length, canARxMessage.data[0]);
    TEST_ASSERT_EQUAL(ESP_OK, canA->Receive(&canARxMessage, CAN_TIMEOUT_TICKS));

    //Check if the message is the same as the one sent.
    TEST_ASSERT_EQUAL(canBMessage.id, canARxMessage.id);
    TEST_ASSERT_EQUAL(canBMessage.length, canARxMessage.length);
    TEST_ASSERT_EQUAL(canBMessage.data[0], canARxMessage.data[0]);
}

// #define MANUAL_CONFIGURATION

extern "C" void app_main(void)
{
    //Wait for the serial port to be opened.
    vTaskDelay(pdMS_TO_TICKS(2000));
    #ifdef MANUAL_CONFIGURATION
    setup();
    #endif
    UNITY_BEGIN();
    RUN_TEST([]() { TwoWayTest("SPI->TWAI", spiCAN, twaiCAN); });
    RUN_TEST([]() { TwoWayTest("TWAI->SPI", twaiCAN, spiCAN); });
    RUN_TEST([]() { FourWayTest("SPI->SPI", spiCAN, twaiCAN); });
    RUN_TEST([]() { FourWayTest("TWAI->SPI", twaiCAN, spiCAN); });
    UNITY_END();
    #ifdef MANUAL_CONFIGURATION
    teardown();
    #endif
}
