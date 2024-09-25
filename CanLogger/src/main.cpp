//https://community.platformio.org/t/esp-log-not-working-on-wemos-s2-mini-but-fine-on-s3-devkit/34717/4
#include <Arduino.h>
#ifdef LOG_LOCAL_LEVEL
#undef LOG_LOCAL_LEVEL
#endif
#define LOG_LOCAL_LEVEL  ESP_LOG_VERBOSE    // this overrides CONFIG_LOG_MAXIMUM_LEVEL setting in menuconfig
                                            // and must be defined before including esp_log.h
#include "esp_log.h"

#include "Logging.hpp" //Include this first so that it takes priority over the ESP-IDF logging macros.
#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/twai.h>
#include <HardwareSerial.h>
#define CAN_TIMEOUT_TICKS pdMS_TO_TICKS(50)

void setup()
{
    esp_log_level_set("*", LOG_LOCAL_LEVEL);

    vTaskDelay(pdMS_TO_TICKS(2000));

    // esp_log_level_set("*", DEBUG_LOG_LEVEL);
    LOG_INFO("Log level set to: %d", esp_log_level_get("*"));

    PRINT("Debug setup started.\n");
    LOG_INFO("Debug setup started.");
    LOG_WARN("Debug setup started.");
    LOG_ERROR("Debug setup started.");
    LOG_DEBUG("Debug setup started.");
    LOG_TRACE("Debug setup started.");

    //Configure GPIO.
    gpio_config_t txPinConfig = {
        .pin_bit_mask = 1ULL << TWAI_TX_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config_t rxPinConfig = {
        .pin_bit_mask = 1ULL << TWAI_RX_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ASSERT(gpio_config(&txPinConfig) == ESP_OK);
    ASSERT(gpio_config(&rxPinConfig) == ESP_OK);

    twai_general_config_t generalConfig = TWAI_GENERAL_CONFIG_DEFAULT(
        TWAI_TX_PIN,
        TWAI_RX_PIN,
        TWAI_MODE_NORMAL
    );
    twai_timing_config_t timingConfig = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t filterConfig = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ASSERT(twai_driver_install(&generalConfig, &timingConfig, &filterConfig) == ESP_OK);
    ASSERT(twai_start() == ESP_OK);
    ASSERT(twai_reconfigure_alerts(TWAI_ALERT_RX_DATA, NULL) == ESP_OK);

    LOG_INFO("Setup completed.");
}

void loop()
{
    // esp_log_level_set("*", LOG_LOCAL_LEVEL);

    uint32_t alerts;
    if (esp_err_t err = twai_read_alerts(&alerts, CAN_TIMEOUT_TICKS) != ESP_OK)
    {
        LOG_TRACE("TWAI Wait Timeout: %d", err);
        return;
    }

    ulong timestamp = xTaskGetTickCount();
    twai_message_t twaiMessage;
    esp_err_t err = twai_receive(&twaiMessage, CAN_TIMEOUT_TICKS);

    if (err != ESP_OK)
    {
        LOG_ERROR("TWAI Read Error: %d", err);
        return;
    }

    for (int i = twaiMessage.data_length_code - 1; i < TWAI_FRAME_MAX_DLC; i++)
        twaiMessage.data[i] = 0;

    PRINT("[CAN]%lu,,%x,%d,%d,%d,%x,%x,%x,%x,%x,%x,%x,%x\n",
        timestamp,
        twaiMessage.identifier,
        twaiMessage.extd,
        twaiMessage.rtr,
        twaiMessage.data_length_code,
        twaiMessage.data[0],
        twaiMessage.data[1],
        twaiMessage.data[2],
        twaiMessage.data[3],
        twaiMessage.data[4],
        twaiMessage.data[5],
        twaiMessage.data[6],
        twaiMessage.data[7]
    );
}
