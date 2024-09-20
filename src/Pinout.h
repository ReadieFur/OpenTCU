#pragma once

#include "hal/gpio_hal.h"

/**
 * Pinout configuration for the MCP2515.
 */
#define MCP_INT_PIN                 GPIO_NUM_NC
#define MCP_SCK_PIN                 GPIO_NUM_NC
#define MCP_MOSI_PIN                GPIO_NUM_NC
#define MCP_MISO_PIN                GPIO_NUM_NC
#define MCP_CS_PIN                  GPIO_NUM_NC

/**
 * Pinout configuration for the TWAI controller.
 */
#define TWAI_TX_PIN                 GPIO_NUM_NC
#define TWAI_RX_PIN                 GPIO_NUM_NC

/**
 * Pinout for GSM module.
 * TODO: Future add-in for remote logging. https://lastminuteengineers.com/sim800l-gsm-module-arduino-tutorial/
 */
#define GSM_TX_PIN                  GPIO_NUM_NC
#define GSM_RX_PIN                  GPIO_NUM_NC
#define GSM_RST_PIN                 GPIO_NUM_NC

/**
 * Pinout for GPS module.
 * TODO: Future add-in for location tracking. https://lastminuteengineers.com/neo6m-gps-arduino-tutorial/
 */
#define GPS_TX_PIN                  GPIO_NUM_NC
#define GPS_RX_PIN                  GPIO_NUM_NC

/**
 * Pinout for Gyroscope module.
 * TODO: Future add-in for motion detection. https://lastminuteengineers.com/mpu6050-accel-gyro-arduino-tutorial/
 */
#define GYRO_SCL_PIN                GPIO_NUM_NC
#define GYRO_SDA_PIN                GPIO_NUM_NC
#define GYRO_INT_PIN                GPIO_NUM_NC
