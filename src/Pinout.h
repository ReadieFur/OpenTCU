#pragma once

#include "hal/gpio_hal.h"

/**
 * Pinout configuration for the MCP2515.
 */
#define MCP_INT_PIN                 GPIO_NUM_02
#define MCP_SCK_PIN                 GPIO_NUM_15
#define MCP_MOSI_PIN                GPIO_NUM_14
#define MCP_MISO_PIN                GPIO_NUM_13
#define MCP_CS_PIN                  GPIO_NUM_18

/**
 * Pinout configuration for the TWAI controller.
 */
#define TWAI_TX_PIN                 GPIO_NUM_32
#define TWAI_RX_PIN                 GPIO_NUM_34

/**
 * Pinout for display module.
 * TODO: Future add-in for HUD.
 */
#define DISP_SCL_PIN                GPIO_NUM_05
#define DISP_SDA_PIN                GPIO_NUM_39

/**
 * Pinout for GSM module.
 * TODO: Future add-in for remote logging. https://lastminuteengineers.com/sim800l-gsm-module-arduino-tutorial/
 */
#define GSM_TX_PIN                  GPIO_NUM_26
#define GSM_RX_PIN                  GPIO_NUM_27
#define GSM_RST_PIN                 GPIO_NUM_5

/**
 * Pinout for GPS module.
 * TODO: Future add-in for location tracking. https://lastminuteengineers.com/neo6m-gps-arduino-tutorial/
 */
#define GPS_TX_PIN                  GPIO_NUM_21
#define GPS_RX_PIN                  GPIO_NUM_22

/**
 * Pinout for gyroscope module.
 * TODO: Future add-in for motion detection. https://lastminuteengineers.com/mpu6050-accel-gyro-arduino-tutorial/
 */
#define GYRO_SCL_PIN                GPIO_NUM_05
#define GYRO_SDA_PIN                GPIO_NUM_39
#define GYRO_INT_PIN                GPIO_NUM_0

//TODO: Mux these pins or see what can be combined due to running out of IO.
