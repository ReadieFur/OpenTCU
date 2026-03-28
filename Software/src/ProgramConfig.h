#pragma once

#include <hal/gpio_hal.h>
#include <hal/adc_hal.h>
#include <soc/soc_caps.h>

// ==== GPIO Pins ====
/**
 * Pinout configuration for the TWAI controller(s).
 */
#define TWAI1_TX_PIN                 GPIO_NUM_7
#define TWAI1_RX_PIN                 GPIO_NUM_6
#define TWAI2_TX_PIN                 GPIO_NUM_5                 //If not using a second TWAI controller, use the MCP2515.
#define TWAI2_RX_PIN                 GPIO_NUM_4                 //If not using a second TWAI controller, use the MCP2515.

/**
 * Pinout configuration for the MCP2515 (if being used).
 */
#if SOC_TWAI_CONTROLLER_NUM < 2
#define MCP_INT_PIN                 GPIO_NUM_2
#define MCP_SCK_PIN                 GPIO_NUM_15
#define MCP_MOSI_PIN                GPIO_NUM_14
#define MCP_MISO_PIN                GPIO_NUM_13
#define MCP_CS_PIN                  GPIO_NUM_18
#endif

/**
 * Misc pinout configuration.
 */
#define LED_PIN                     GPIO_NUM_15
// #define WS2812B_PIN                 GPIO_NUM_8

// ==== TCU Parameters ====
// #define TCU_NAME                    "WSBC..."
#define TCU_CODE                     123456                     //TODO: Move this to the platformio config

// ==== Debug Options ====
#ifdef DEBUG
// #define CAN_DUMP_LIVE
// #define CAN_DUMP_SERIAL
#define CAN_DUMP_UDP
#define CAN_DUMP_BEFORE_INTERCEPT
// #define CAN_DUMP_AFTER_INTERCEPT
#endif

// ==== Conditionals ====
#if defined(CAN_DUMP_SERIAL) || defined(CAN_DUMP_UDP)
#define CAN_DUMP
#endif
