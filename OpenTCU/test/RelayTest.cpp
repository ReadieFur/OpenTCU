#define _TEST

#include <freertos/FreeRTOS.h>
#include "Helpers.hpp"
#include <unity.h>
#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <driver/spi_common.h>
#include <driver/twai.h>
#include <esp_random.h>
#include <BusMaster.hpp>

#define CAN_TIMEOUT_TICKS pdMS_TO_TICKS(50)
//Ideally I would want the delay to be 0 but it is unpredictable how long it will take for the CAN controllers to process the data, it seems like the upper limit of the random delay is 2ms.
#define TEST_DELAY() vTaskDelay(pdMS_TO_TICKS(2))
// #define TEST_DELAY()

#ifndef MANUAL_CONFIGURATION
void setUp(void)
#else
void setup()
#endif
{
    esp_log_level_set("*", ESP_LOG_VERBOSE);
    BusMaster::Init();
    BusMaster::Stop();
}

#ifndef MANUAL_CONFIGURATION
void tearDown(void)
#else
void teardown()
#endif
{
    BusMaster::Destroy();
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
    RUN_TEST([]() { TwoWayTest("SPI->TWAI", BusMaster::spiCan, BusMaster::twaiCan); });
    RUN_TEST([]() { TwoWayTest("TWAI->SPI", BusMaster::twaiCan, BusMaster::spiCan); });
    RUN_TEST([]() { FourWayTest("SPI->SPI", BusMaster::spiCan, BusMaster::twaiCan); });
    RUN_TEST([]() { FourWayTest("TWAI->SPI", BusMaster::twaiCan, BusMaster::spiCan); });
    UNITY_END();
    #ifdef MANUAL_CONFIGURATION
    teardown();
    #endif
}
