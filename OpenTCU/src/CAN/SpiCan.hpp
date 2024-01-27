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
#include "Helpers.h"
#if DEBUG
#include <esp_log.h>
#endif

class SpiCan : public ACan
{
private:
    spi_device_handle_t device;
    MCP2515* mcp2515;
    gpio_num_t interruptPin;
    volatile SemaphoreHandle_t interruptSemaphore = xSemaphoreCreateCounting(2, 0); //2 because the MCP2515 has two buffers (RX0 and RX1).

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
        #if defined(DEBUG) && 0
        isr_log_v("SpiCan", "OnInterrupt");
        #endif

        BaseType_t higherPriorityTaskWoken = pdFALSE;

        //No need to check the arg type given I know what it is (optimization).
        //Notify the task that a message has been received.
        xSemaphoreGiveFromISR(static_cast<SpiCan*>(arg)->interruptSemaphore, &higherPriorityTaskWoken);

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
        this->device = device;
        mcp2515 = new MCP2515(&this->device);
        if (uint8_t res = mcp2515->reset() != MCP2515::ERROR_OK)
            throw std::runtime_error("Failed to reset MCP2515: " + std::to_string(res));
        if (uint8_t res = mcp2515->setBitrate(speed, clock) != MCP2515::ERROR_OK)
            throw std::runtime_error("Failed to set bitrate: " + std::to_string(res));
        if (uint8_t res = mcp2515->setNormalMode() != MCP2515::ERROR_OK) //TODO: Allow the user to change this setting.
            throw std::runtime_error("Failed to set normal mode: " + std::to_string(res));

        if (interruptPin == GPIO_NUM_NC)
            throw std::runtime_error("Interrupt pin cannot be GPIO_NUM_NC.");
        this->interruptPin = interruptPin;
        //It seems like this method returns an error all of the time, however it is safe to call again. If something truly bad happens we will likely throw in the next stage.
        //https://esp32.com/viewtopic.php?t=13167
        gpio_install_isr_service(0);
        if (esp_err_t res = gpio_isr_handler_add(interruptPin, OnInterrupt, this) != ESP_OK)
            throw std::runtime_error("Failed to add ISR handler: " + std::to_string(res));

        //Read the initial state of the interrupt pin.
        TRACE("Initial interrupt pin state: %d", gpio_get_level(interruptPin));
        if (gpio_get_level(interruptPin) == 0)
        {
            xSemaphoreGive(interruptSemaphore);
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

        // TRACE("SPI Write");
        if (xSemaphoreTake(driverMutex, timeout) != pdTRUE)
        {
            // TRACE("SPI Write Timeout");
            return ESP_ERR_TIMEOUT;
        }
        MCP2515::ERROR res = mcp2515->sendMessage(&frame);
        // TRACE("SPI Write Done");
        xSemaphoreGive(driverMutex);
        
        return MCPErrorToESPError(res);
    }

    esp_err_t Receive(SCanMessage* message, TickType_t timeout = 0)
    {
        #if defined(DEBUG) && 0
        //Get status.
        uint8_t a = mcp2515->checkReceive();
        uint8_t b = mcp2515->checkError();
        uint8_t c = mcp2515->getErrorFlags();
        uint8_t d = mcp2515->getInterrupts();
        uint8_t e = mcp2515->getInterruptMask();

        //Convert error flags to enum values.
        bool rx0Ovr = c & MCP2515::EFLG_RX0OVR;
        bool rx1Ovr = c & MCP2515::EFLG_RX1OVR;
        bool txBO = c & MCP2515::EFLG_TXBO;
        bool txEP = c & MCP2515::EFLG_TXEP;
        bool rxEP = c & MCP2515::EFLG_RXEP;
        bool txWAR = c & MCP2515::EFLG_TXWAR;
        bool rxWAR = c & MCP2515::EFLG_RXWAR;
        bool ewarn = c & MCP2515::EFLG_EWARN;

        //CANINTF
        bool rx0if = e & MCP2515::CANINTF_RX0IF;
        bool rx1if = e & MCP2515::CANINTF_RX1IF;
        bool tx0if = e & MCP2515::CANINTF_TX0IF;
        bool tx1if = e & MCP2515::CANINTF_TX1IF;
        bool tx2if = e & MCP2515::CANINTF_TX2IF;
        bool errif = e & MCP2515::CANINTF_ERRIF;
        bool wakif = e & MCP2515::CANINTF_WAKIF;
        bool merrf = e & MCP2515::CANINTF_MERRF;

        TRACE("SPI Status: RX: %d, ERR: %d, ERR Flags: %d %d %d %d %d %d %d %d, INT: %d, INT Mask: %d %d %d %d %d %d %d %d",
            a, b, rx0Ovr, rx1Ovr, txBO, txEP, rxEP, txWAR, rxWAR, ewarn,
            d, rx0if, rx1if, tx0if, tx1if, tx2if, errif, wakif, merrf);
        #endif
        
        //Check if we need to wait for a message to be received.
        if (gpio_get_level(interruptPin) == 1)
        {
            // TRACE("SPI Wait: %d, %d", uxSemaphoreGetCount(interruptSemaphore), gpio_get_level(interruptPin));
            //Wait in a "non-blocking" manner by allowing the CPU to do other things while waiting for a message.
            if (xSemaphoreTake(interruptSemaphore, timeout) != pdTRUE)
            {
                // TRACE("SPI Wait Timeout");
                return ESP_ERR_TIMEOUT;
            }
        }
        else
        {
            //Attempt to decrement the semaphore count but don't wait or check the result (helps prevent overflow).
            xSemaphoreTake(interruptSemaphore, 0);
        }

        // TRACE("SPI Read");
        can_frame frame;
        //Lock the driver from other operations while we read the message.
        if (xSemaphoreTake(driverMutex, timeout) != pdTRUE)
        {
            // TRACE("SPI Read Timeout");
            return ESP_ERR_TIMEOUT;
        }

        //https://github.com/autowp/arduino-canhacker/blob/master/CanHacker.cpp#L216-L271
        uint8_t interruptFlags = mcp2515->getInterrupts();

        if (interruptFlags & MCP2515::CANINTF_ERRIF)
            mcp2515->clearRXnOVR();

        MCP2515::ERROR readResult;
        if (interruptFlags & MCP2515::CANINTF_RX0IF)
            readResult = mcp2515->readMessage(MCP2515::RXB0, &frame);
        else if (interruptFlags & MCP2515::CANINTF_RX1IF)
            readResult = mcp2515->readMessage(MCP2515::RXB1, &frame);
        else
            readResult = MCP2515::ERROR_NOMSG;

        if (interruptFlags & MCP2515::CANINTF_WAKIF)
            mcp2515->clearInterrupts();

        if (interruptFlags & MCP2515::CANINTF_ERRIF)
            mcp2515->clearMERR();

        if (interruptFlags & MCP2515::CANINTF_MERRF)
            mcp2515->clearInterrupts();

        xSemaphoreGive(driverMutex);
        if (readResult != MCP2515::ERROR_OK)
        {
            //At some point in this development I broke the interrupt and it seems it never fires now.
            //As a result of I am using gpio_get_level. However an issue has occurred where I can reach this point and read empty messages (error code 5).
            //I would like to fix this as we are wasting CPU cycles with this bug.
            TRACE("SPI Read Error: %d", readResult);
            return MCPErrorToESPError(readResult);
        }
        // TRACE("SPI Read Success");

        message->id = frame.can_id & (frame.can_id & CAN_EFF_FLAG ? CAN_EFF_MASK : CAN_SFF_MASK);
        message->length = frame.can_dlc;
        message->isExtended = frame.can_id & CAN_EFF_FLAG;
        message->isRemote = frame.can_id & CAN_RTR_FLAG;
        for (int i = 0; i < message->length; i++)
            message->data[i] = frame.data[i];

        return ESP_OK;
    }
};
