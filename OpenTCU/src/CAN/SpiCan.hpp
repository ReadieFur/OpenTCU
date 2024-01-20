#pragma once

#include "ACan.hpp"
#include <driver/spi_master.h>
#include <mcp2515.h>
#include <stdexcept>
#include <esp_intr_alloc.h>
#include <esp_attr.h>
#include <freertos/semphr.h>
#include <driver/gpio.h>
#if DEBUG
#include <esp_log.h>
#endif

class SpiCan : public ACan
{
private:
    spi_device_handle_t device;
    MCP2515* mcp2515;
    gpio_num_t interruptPin;
    SemaphoreHandle_t interruptSemaphore = xSemaphoreCreateBinary();

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

    static void IRAM_ATTR OnInterrupt(void* arg)
    {
        #if DEBUG
        ESP_DRAM_LOGV("SpiCan", "OnInterrupt");
        #endif

        //Check if the arg is an instance of SpiCan.
        if (arg == NULL || !dynamic_cast<SpiCan*>(static_cast<SpiCan*>(arg)))
            return;

        //Notify the task that a message has been received.
        xSemaphoreGiveFromISR(static_cast<SpiCan*>(arg)->interruptSemaphore, NULL);
    }

public:
    SpiCan(spi_device_handle_t device, CAN_SPEED speed, CAN_CLOCK clock, gpio_num_t interruptPin = GPIO_NUM_NC)
    {
        //Keep a reference to the device (required for the MCP2515 library otherwise it will crash the device).
        //We are passing an instance here so that the instance does not need to be stored outside of this class.
        this->device = device;
        mcp2515 = new MCP2515(&this->device);
        if (uint8_t res = mcp2515->reset() != MCP2515::ERROR_OK)
            throw std::runtime_error("Failed to reset MCP2515: " + std::to_string(res));
        if (uint8_t res = mcp2515->setBitrate(speed, clock) != MCP2515::ERROR_OK)
            throw std::runtime_error("Failed to set bitrate: " + std::to_string(res));
        #ifndef GENERATOR
        if (uint8_t res = mcp2515->setNormalMode() != MCP2515::ERROR_OK) //TODO: Allow the user to change this setting.
            throw std::runtime_error("Failed to set normal mode: " + std::to_string(res));
        #else
        if (uint8_t res = mcp2515->setLoopbackMode() != MCP2515::ERROR_OK) //TODO: Allow the user to change this setting.
            throw std::runtime_error("Failed to set loopback mode: " + std::to_string(res));
        #endif

        this->interruptPin = interruptPin;
        if (interruptPin != GPIO_NUM_NC)
        {
            gpio_config_t config = {
                .pin_bit_mask = 1ULL << interruptPin,
                .mode = GPIO_MODE_INPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_POSEDGE
            };
            //If the result of the install is not OK or INVALID_STATE, throw an error. (Invalid state means that the ISR service is already installed).
            // if (esp_err_t res = gpio_install_isr_service(0) != ESP_OK || res != ESP_ERR_INVALID_STATE)
            //     throw std::runtime_error("Failed to install ISR service: " + std::to_string(res));
            //It seems like this method returns an error all of the time, however it is safe to call again. If something truly bad happens we will likely throw in the next stage.
            //https://esp32.com/viewtopic.php?t=13167
            gpio_install_isr_service(0);
            if (esp_err_t res = gpio_config(&config) != ESP_OK)
                throw std::runtime_error("Failed to configure interrupt pin: " + std::to_string(res));
            if (esp_err_t res = gpio_isr_handler_add(interruptPin, OnInterrupt, this) != ESP_OK)
                throw std::runtime_error("Failed to add ISR handler: " + std::to_string(res));
        }
    }

    ~SpiCan()
    {
        gpio_isr_handler_remove(interruptPin);
        delete mcp2515;
        //Release the semaphore incase it is taken (this will cause an error if something is waiting on it, but it's best to error and catch rather than hang).
        xSemaphoreGive(interruptSemaphore);
    }

    esp_err_t Send(SCanMessage message, TickType_t timeout = 0)
    {
        can_frame frame = {
            .can_id = message.id | (message.isExtended ? CAN_EFF_FLAG : 0) | (message.isRemote ? CAN_RTR_FLAG : 0),
            .can_dlc = message.length
        };
        for (int i = 0; i < message.length; i++)
            frame.data[i] = message.data[i];

        return MCPErrorToESPError(mcp2515->sendMessage(&frame));
    }

    esp_err_t Receive(SCanMessage* message, TickType_t timeout = 0)
    {
        // //Wait in a "non-blocking" by allowing the CPU to do other things while waiting for a message.
        // BaseType_t res = xSemaphoreTake(interruptSemaphore, timeout);
        // //Check if we timed out.
        // if (res == pdFALSE)
        //     return ESP_ERR_TIMEOUT;

        can_frame frame;
        MCP2515::ERROR result;
        if (result != MCP2515::ERROR_OK)
            return MCPErrorToESPError(result);

        message->id = frame.can_id & (frame.can_id & CAN_EFF_FLAG ? CAN_EFF_MASK : CAN_SFF_MASK);
        message->length = frame.can_dlc;
        message->isExtended = frame.can_id & CAN_EFF_FLAG;
        message->isRemote = frame.can_id & CAN_RTR_FLAG;
        for (int i = 0; i < message->length; i++)
            message->data[i] = frame.data[i];

        return ESP_OK;
    }

    bool HasAlerts()
    {
        return mcp2515->checkReceive() || mcp2515->checkError();
    }
};
