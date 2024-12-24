#pragma once

#include <hal/gpio_hal.h>
#include <hal/adc_hal.h>

/**
 * TCU parameters.
 */
// #define TCU_NAME                    "WSBC..."
#define TCU_CODE                     123456

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
// #define MCP_INT_PIN                 GPIO_NUM_2
// #define MCP_SCK_PIN                 GPIO_NUM_15
// #define MCP_MOSI_PIN                GPIO_NUM_14
// #define MCP_MISO_PIN                GPIO_NUM_13
// #define MCP_CS_PIN                  GPIO_NUM_18
