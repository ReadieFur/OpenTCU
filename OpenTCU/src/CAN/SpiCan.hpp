#pragma once

#include "ACan.hpp"
#include <driver/spi_master.h>
#include <mcp2515.h>
#include <stdexcept>

class SpiCan : public ACan
{
private:
    spi_device_handle_t device;
    MCP2515* mcp2515;

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

public:
    SpiCan(spi_device_handle_t device, CAN_SPEED speed, CAN_CLOCK clock)
    {
        //Keep a reference to the device (required for the MCP2515 library otherwise it will crash the device).
        //We are passing an instance here so that the instance does not need to be stored outside of this class.
        this->device = device;
        mcp2515 = new MCP2515(&this->device);
        if (mcp2515->reset() != MCP2515::ERROR_OK)
            throw std::runtime_error("Failed to reset MCP2515.");
        if (mcp2515->setBitrate(speed, clock) != MCP2515::ERROR_OK)
            throw std::runtime_error("Failed to set bitrate.");
        if (mcp2515->setNormalMode() != MCP2515::ERROR_OK) //TODO: Allow the user to change this setting.
            throw std::runtime_error("Failed to set normal mode.");
    }

    ~SpiCan()
    {
        delete mcp2515;
    }

    esp_err_t Send(SCanMessage message, TickType_t timeout = 0)
    {
        can_frame frame = {
            .can_id = message.id | (message.isExtended ? CAN_EFF_FLAG : 0) | (message.isRemote ? CAN_RTR_FLAG : 0),
            .can_dlc = message.len
        };
        for (int i = 0; i < message.len; i++)
            frame.data[i] = message.data[i];

        return MCPErrorToESPError(mcp2515->sendMessage(&frame));
    }

    esp_err_t Receive(SCanMessage &message, TickType_t timeout = 0)
    {
        can_frame frame;
        MCP2515::ERROR result;
        int64_t start = esp_timer_get_time();
        
        do result = mcp2515->readMessage(&frame);
        while (result == MCP2515::ERROR_NOMSG && (esp_timer_get_time() - start) < timeout);
        
        if (result != MCP2515::ERROR_OK)
            return MCPErrorToESPError(result);

        message.id = frame.can_id & (frame.can_id & CAN_EFF_FLAG ? CAN_EFF_MASK : CAN_SFF_MASK);
        message.len = frame.can_dlc;
        message.isExtended = frame.can_id & CAN_EFF_FLAG;
        message.isRemote = frame.can_id & CAN_RTR_FLAG;
        for (int i = 0; i < message.len; i++)
            message.data[i] = frame.data[i];

        return ESP_OK;
    }

    bool HasAlerts()
    {
        return mcp2515->checkReceive() || mcp2515->checkError();
    }
};
