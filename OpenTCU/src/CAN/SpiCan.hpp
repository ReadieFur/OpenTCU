#pragma once

#include <driver/spi_master.h>
#include <mcp2515.h>
#include <stdexcept>
#include <esp_intr_alloc.h>
#include <esp_attr.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <driver/gpio.h>
#include "ACan.h"
#include "SCanMessage.h"
#include "Helpers.hpp"
#if DEBUG
#include <esp_log.h>
#endif

class SpiCan : public ACan
{
private:
    spi_device_handle_t _device;
    MCP2515* _mcp2515;
    gpio_num_t _interruptPin;
    volatile SemaphoreHandle_t _interruptSemaphore = xSemaphoreCreateCounting(2, 0); //2 because the MCP2515 has two buffers (RX0 and RX1).

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
        #ifdef VERY_VERBOSE
        isr_log_v("SpiCan", "OnInterrupt");
        #endif

        BaseType_t higherPriorityTaskWoken = pdFALSE;

        //No need to check the arg type given I know what it is (optimization).
        //Notify the task that a message has been received.
        xSemaphoreGiveFromISR(static_cast<SpiCan*>(arg)->_interruptSemaphore, &higherPriorityTaskWoken);

        //If a higher priority task was woken, yield to it.
        //I don't really understand the purpose of this, but it seems to be a common practice.
        if (higherPriorityTaskWoken == pdTRUE)
            portYIELD_FROM_ISR();
    }

public:
    SpiCan(spi_device_handle_t device, CAN_SPEED speed, CAN_CLOCK clock, gpio_num_t interruptPin) : ACan()
    {
        //Keep a reference to the device (required for the MCP2515 library otherwise it will crash the device).
        //We are passing an instance here so that the instance does not need to be stored outside of this class.
        this->_device = device;
        this->_interruptPin = interruptPin;

        _mcp2515 = new MCP2515(&this->_device);

        #ifdef CONFIG_COMPILER_CXX_EXCEPTIONS
        if (uint8_t res = _mcp2515->reset() != MCP2515::ERROR_OK)
            throw std::runtime_error("Failed to reset MCP2515: " + std::to_string(res));
        if (uint8_t res = _mcp2515->setBitrate(speed, clock) != MCP2515::ERROR_OK)
            throw std::runtime_error("Failed to set bitrate: " + std::to_string(res));
        if (uint8_t res = _mcp2515->setNormalMode() != MCP2515::ERROR_OK) //TODO: Allow the user to change this setting.
            throw std::runtime_error("Failed to set normal mode: " + std::to_string(res));
        #else
        ASSERT(mcp2515->reset() == MCP2515::ERROR_OK);
        ASSERT(mcp2515->setBitrate(speed, clock) == MCP2515::ERROR_OK);
        ASSERT(mcp2515->setNormalMode() == MCP2515::ERROR_OK);
        #endif

        //It seems like this method returns an error all of the time, however it is safe to call again. If something truly bad happens we will likely throw in the next stage.
        //https://esp32.com/viewtopic.php?t=13167
        gpio_install_isr_service(0);
        #if CONFIG_COMPILER_CXX_EXCEPTIONS
        if (esp_err_t res = gpio_isr_handler_add(interruptPin, OnInterrupt, this) != ESP_OK)
            throw std::runtime_error("Failed to add ISR handler: " + std::to_string(res));
        #else
        ASSERT(gpio_isr_handler_add(interruptPin, OnInterrupt, this) == ESP_OK);
        #endif

        #ifdef VERY_VERBOSE
        //Read the initial state of the interrupt pin.
        TRACE("Initial interrupt pin state: %d", gpio_get_level(interruptPin));
        #endif
        if (gpio_get_level(interruptPin) == 0)
            xSemaphoreGive(_interruptSemaphore);
    }

    ~SpiCan()
    {
        gpio_isr_handler_remove(_interruptPin);
        
        _mcp2515->reset();
        delete _mcp2515;

        //Release the semaphore incase it is taken (this will cause an error if something is waiting on it, but it's best to error and catch rather than hang).
        xSemaphoreGive(_interruptSemaphore);
    }

    esp_err_t Send(SCanMessage message, TickType_t timeout = 0)
    {
        can_frame frame = {
            .can_id = message.id | (message.isExtended ? CAN_EFF_FLAG : 0) | (message.isRemote ? CAN_RTR_FLAG : 0),
            .can_dlc = message.length
        };
        for (int i = 0; i < message.length; i++)
            frame.data[i] = message.data[i];

        #ifdef VERY_VERBOSE
        TRACE("SPI Write: %x, %d, %d, %d, %x", frame.can_id, frame.can_dlc, frame.can_id & CAN_EFF_FLAG, frame.can_id & CAN_RTR_FLAG, frame.data[0]);
        #endif
        
        #ifdef USE_DRIVER_LOCK
        if (xSemaphoreTake(_driverMutex, timeout) != pdTRUE)
        {
            TRACE("SPI Write Timeout");
            return ESP_ERR_TIMEOUT;
        }
        #endif
        MCP2515::ERROR res = _mcp2515->sendMessage(&frame);
        
        #ifdef VERY_VERBOSE
        TRACE("SPI Write Done");
        #endif
        
        #ifdef USE_DRIVER_LOCK
        xSemaphoreGive(_driverMutex);
        #endif
        
        return MCPErrorToESPError(res);
    }

    esp_err_t Receive(SCanMessage* message, TickType_t timeout = 0)
    {
        //Check if we need to wait for a message to be received.
        if (gpio_get_level(_interruptPin) == 1)
        {
            #ifdef VERY_VERBOSE
            TRACE("SPI Wait: %d, %d", uxSemaphoreGetCount(interruptSemaphore), gpio_get_level(interruptPin));
            #endif

            //Wait in a "non-blocking" manner by allowing the CPU to do other things while waiting for a message.
            if (xSemaphoreTake(_interruptSemaphore, timeout) != pdTRUE)
            {
                #ifdef VERY_VERBOSE
                TRACE("SPI Wait Timeout");
                #endif
                return ESP_ERR_TIMEOUT;
            }
        }
        else
        {
            //Attempt to decrement the semaphore count but don't wait or check the result (helps prevent overflow).
            xSemaphoreTake(_interruptSemaphore, 0);
        }

        #ifdef VERY_VERBOSE
        TRACE("SPI Read");
        #endif

        #ifdef USE_DRIVER_LOCK
        //Lock the driver from other operations while we read the message.
        if (xSemaphoreTake(_driverMutex, timeout) != pdTRUE)
        {
            TRACE("SPI Read Timeout");
            return ESP_ERR_TIMEOUT;
        }
        #endif

        //https://github.com/autowp/arduino-canhacker/blob/master/CanHacker.cpp#L216-L271
        //https://ww1.microchip.com/downloads/en/DeviceDoc/MCP2515-Stand-Alone-CAN-Controller-with-SPI-20001801J.pdf#54
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

        #ifdef USE_DRIVER_LOCK
        xSemaphoreGive(_driverMutex);
        #endif

        if (readResult != MCP2515::ERROR_OK)
        {
            //At some point in this development I broke the interrupt and it seems it never fires now.
            //As a result of I am using gpio_get_level. However an issue has occurred where I can reach this point and read empty messages (error code 5).
            //I would like to fix this as we are wasting CPU cycles with this bug.
            TRACE("SPI Read Error: %d", readResult);
            return MCPErrorToESPError(readResult);
        }

        #ifdef VERY_VERBOSE
        TRACE("SPI Read Success: %x, %d, %d, %d, %x", frame.can_id, frame.can_dlc, frame.can_id & CAN_EFF_FLAG, frame.can_id & CAN_RTR_FLAG, frame.data[0]);
        #endif

        message->id = frame.can_id & (frame.can_id & CAN_EFF_FLAG ? CAN_EFF_MASK : CAN_SFF_MASK);
        message->length = frame.can_dlc;
        message->isExtended = frame.can_id & CAN_EFF_FLAG;
        message->isRemote = frame.can_id & CAN_RTR_FLAG;
        for (int i = 0; i < message->length; i++)
            message->data[i] = frame.data[i];

        return ESP_OK;
    }

    esp_err_t GetStatus(uint32_t* status, TickType_t timeout = 0)
    {
        #ifdef USE_DRIVER_LOCK
        if (xSemaphoreTake(_driverMutex, timeout) != pdTRUE)
        {
            #ifdef VERY_VERBOSE
            TRACE("SPI Status Timeout");
            #endif
            return ESP_ERR_TIMEOUT;
        }
        #endif
        *status = _mcp2515->getInterrupts();
        #ifdef USE_DRIVER_LOCK
        xSemaphoreGive(_driverMutex);
        #endif

        #if defined(VERY_VERBOSE)
        TRACE("SPI Status: %d %d %d %d %d %d %d %d",
            *status & MCP2515::CANINTF_RX0IF, //If the result is non-zero then the interrupt has fired.
            *status & MCP2515::CANINTF_RX1IF,
            *status & MCP2515::CANINTF_TX0IF,
            *status & MCP2515::CANINTF_TX1IF,
            *status & MCP2515::CANINTF_TX2IF,
            *status & MCP2515::CANINTF_ERRIF,
            *status & MCP2515::CANINTF_WAKIF,
            *status & MCP2515::CANINTF_MERRF);
        #endif

        return ESP_OK;
    }
};
