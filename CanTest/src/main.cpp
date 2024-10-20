// #define TEST_CHIP

//https://community.platformio.org/t/esp-log-not-working-on-wemos-s2-mini-but-fine-on-s3-devkit/34717/4
#include <Arduino.h>
#define MAX_LOG_LEVEL ESP_LOG_DEBUG
#ifdef LOG_LOCAL_LEVEL
#undef LOG_LOCAL_LEVEL
#endif
#define LOG_LOCAL_LEVEL MAX_LOG_LEVEL //This overrides CONFIG_LOG_MAXIMUM_LEVEL setting in menuconfig and must be defined before including esp_log.h
#ifdef CONFIG_LOG_MAXIMUM_LEVEL
#undef CONFIG_LOG_MAXIMUM_LEVEL
#endif
#define CONFIG_LOG_MAXIMUM_LEVEL MAX_LOG_LEVEL
#include <esp_log.h>

#ifndef TEST_CHIP
#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include <freertos/task.h>
#endif

#include <cstdint>
#include <esp_err.h>
#include <driver/gpio.h>
#include <driver/twai.h>
#include <stdexcept>
#include <esp_check.h>
#include <esp_log.h>
#include <driver/spi_master.h>
#include <mcp2515.h>
#include <stdexcept>
#include <esp_intr_alloc.h>
#include <esp_attr.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <driver/gpio.h>

void SetLogLevel()
{
    //These seem to do nothing :/
    esp_log_level_set("test", ESP_LOG_DEBUG);
    esp_log_level_set("spi", ESP_LOG_VERBOSE);
    esp_log_level_set("twai", ESP_LOG_NONE);
    esp_log_level_set("bus", ESP_LOG_ERROR);
    esp_log_level_set("dbg", ESP_LOG_DEBUG);
    esp_log_level_set("can", ESP_LOG_INFO);
}

spi_device_handle_t _spiDevice;
MCP2515* _mcp2515;

static esp_err_t MCPErrorToESPError(MCP2515::ERROR error)
{
    switch (error)
    {
    case MCP2515::ERROR_OK:
        return ESP_OK;
    case MCP2515::ERROR_ALLTXBUSY:
    case MCP2515::ERROR_FAILINIT:
        return ESP_ERR_INVALID_STATE;
    case MCP2515::ERROR_NOMSG:
        return ESP_ERR_INVALID_STATE;
    case MCP2515::ERROR_FAIL:
    case MCP2515::ERROR_FAILTX:
    default:
        return ESP_FAIL;
    }
}

void setup()
{
    Serial.begin(115200);
    vTaskDelay(pdMS_TO_TICKS(5000));

    SetLogLevel();

    xTaskCreate([](void*)
    {
        while(true)
        {
            // SetLogLevel();
            ESP_LOGD("test", "SERIAL_ALIVE_CHECK");
            vTaskDelay(pdMS_TO_TICKS(10 * 1000));
        }
    }, "AliveCheck", configIDLE_TASK_STACK_SIZE + 1024, nullptr, (configMAX_PRIORITIES * 0.35), nullptr);
    //It is critical that this is the first service to be started.

    //TWAI setup.
    pinMode(TWAI_RX_PIN, INPUT);
    pinMode(TWAI_TX_PIN, OUTPUT);
    twai_general_config_t generalConfig = TWAI_GENERAL_CONFIG_DEFAULT(
        TWAI_TX_PIN,
        TWAI_RX_PIN,
        TWAI_MODE_NORMAL
    );
    twai_timing_config_t timingConfig = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t filterConfig = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    ESP_ERROR_CHECK(twai_driver_install(&generalConfig, &timingConfig, &filterConfig));
    ESP_ERROR_CHECK(twai_start());
    ESP_ERROR_CHECK(twai_reconfigure_alerts(TWAI_ALERT_RX_DATA, NULL));

    //SPI setup.
    pinMode(SPI_MISO_PIN, INPUT);
    pinMode(SPI_MOSI_PIN, OUTPUT);
    pinMode(SPI_CS_PIN, OUTPUT);
    pinMode(SPI_INT_PIN, INPUT);
    spi_bus_config_t busConfig = {
        .mosi_io_num = SPI_MOSI_PIN,
        .miso_io_num = SPI_MISO_PIN,
        .sclk_io_num = SPI_SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE,
    };
    //SPI2_HOST is the only SPI bus that can be used as GPSPI on the C3.
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &busConfig, SPI_DMA_CH_AUTO));
    spi_device_interface_config_t dev_config = {
        .mode = 0,
        .clock_speed_hz = SPI_MASTER_FREQ_8M, //Match the SPI CAN controller.
        .spics_io_num = SPI_CS_PIN,
        .queue_size = 2, //2 as per the specification: https://ww1.microchip.com/downloads/en/DeviceDoc/MCP2515-Stand-Alone-CAN-Controller-with-SPI-20001801J.pdf
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev_config, &_spiDevice));
    _mcp2515 = new MCP2515(&_spiDevice);
    ESP_ERROR_CHECK(MCPErrorToESPError(_mcp2515->reset()));
    ESP_ERROR_CHECK(MCPErrorToESPError(_mcp2515->setBitrate(CAN_250KBPS, MCP_8MHZ)));
    ESP_ERROR_CHECK(MCPErrorToESPError(_mcp2515->setNormalMode()));
}

#ifdef ARDUINO
void loop()
{
    //Send on CAN.
    twai_message_t twaiMessage = {
        .identifier = 123,
        .data_length_code = 1
    };
    twaiMessage.extd = false;
    twaiMessage.rtr = false;
    twaiMessage.data[0] = millis();
    esp_err_t twaiRes = twai_transmit(&twaiMessage, pdMS_TO_TICKS(500));
    if (twaiRes != ESP_OK)
    {
        ESP_LOGE("twai", "TWAI Write Error: %s", esp_err_to_name(twaiRes));
    }
    else
    {
        ESP_LOGI("twai", "TWAI Written.");
    }
    Serial.println();

    //Read on SPI.
    long start = millis();
    while (gpio_get_level(SPI_INT_PIN) != 1)
    {
        long now = millis();
        if (now - start > 500)
        {
            ESP_LOGE("spi", "SPI Read Timeout");
            break;
        }
    }
    uint8_t interruptFlags = _mcp2515->getInterrupts();
    if (interruptFlags & MCP2515::CANINTF_ERRIF) //Error Interrupt Flag bit is set.
        _mcp2515->clearRXnOVR();
    can_frame frame;
    MCP2515::ERROR readResult;
    if (interruptFlags & MCP2515::CANINTF_RX0IF) //Receive Buffer 0 Full Interrupt Flag bit is set.
        readResult = _mcp2515->readMessage(MCP2515::RXB0, &frame);
    else if (interruptFlags & MCP2515::CANINTF_RX1IF) //Receive Buffer 1 Full Interrupt Flag bit is set.
        readResult = _mcp2515->readMessage(MCP2515::RXB1, &frame);
    else
        readResult = MCP2515::ERROR_NOMSG;
    //I shouldn't need to check this flag as we shouldn't ever be in a sleep mode (for now).
    // if (interruptFlags & MCP2515::CANINTF_WAKIF) Wake-up Interrupt Flag bit is set.
    //     mcp2515->clearInterrupts();
    if (interruptFlags & MCP2515::CANINTF_ERRIF)
        _mcp2515->clearMERR();
    if (interruptFlags & MCP2515::CANINTF_MERRF) //Message Error Interrupt Flag bit is set.
        _mcp2515->clearInterrupts();
    if (readResult != MCP2515::ERROR_OK)
    {
        ESP_LOGE("spi", "SPI Read Error: %s", esp_err_to_name(MCPErrorToESPError(readResult)));
    }
    else
    {
        ESP_LOGI("spi", "SPI Read Message:\nID: %i\nD[0]: %li",
            frame.can_id & (frame.can_id & CAN_EFF_FLAG ? CAN_EFF_MASK : CAN_SFF_MASK),
            frame.data[0]
        );
    }
    Serial.println();

    //Send on SPI.
    can_frame sendFrame = {
        .can_id = 456 | (false ? CAN_EFF_FLAG : 0) | (false ? CAN_RTR_FLAG : 0),
        .can_dlc = 1
    };
    sendFrame.data[0] = millis();
    MCP2515::ERROR mcpRes = _mcp2515->sendMessage(&sendFrame);
    if (mcpRes != MCP2515::ERROR_OK)
    {
        ESP_LOGE("spi", "SPI Write Error: %d", mcpRes);
    }
    else
    {
        ESP_LOGI("spi", "SPI Written.");
    }
    Serial.println();

    //Read on TWAI.
    uint32_t alerts;
    if (twaiRes = twai_read_alerts(&alerts, pdMS_TO_TICKS(500)) != ESP_OK)
    {
        ESP_LOGV("twai", "TWAI Wait Timeout: %d", twaiRes);
    }
    twai_message_t twaiRecvMessage;
    twaiRes = twai_receive(&twaiRecvMessage, pdMS_TO_TICKS(500));
    if (twaiRes != ESP_OK)
    {
        ESP_LOGE("twai", "TWAI Read Error: %s", esp_err_to_name(twaiRes));
    }
    else
    {
        ESP_LOGI("spi", "TWAI Read Message:\nID: %i\nD[0]: %li",
            twaiRecvMessage.identifier,
            twaiRecvMessage.data[0]
        );
    }
    Serial.println();

    vTaskDelay(1000);
}
#else
extern "C" void app_main()
{
    setup();
    //app_main IS allowed to return as per the ESP32 documentation (other FreeRTOS tasks will continue to run).
}
#endif
