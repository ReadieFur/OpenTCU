#pragma once

//This file is meant to replicate the functionality that will be implimented to the BusMaster class from the embedded project code.

#include "Helpers.hpp"
#include <vector>
#include "Samples.hpp"
#include "EStringType.h"

#pragma region Other data
EStringType _stringRequestType = EStringType::None;
size_t _stringRequestBufferIndex = 0;
char* _stringRequestBuffer = nullptr;

uint32_t _wheelCircumference = 0;
#pragma endregion

#pragma region Live data
Samples<uint16_t, uint32_t> _bikeSpeedBuffer = Samples<uint16_t, uint32_t>(10);

bool _walkMode = false;
uint8_t _easeSetting = 0;
uint8_t _powerSetting = 0;

Samples<uint16_t, uint32_t> _batteryVoltage = Samples<uint16_t, uint32_t>(10);
Samples<int32_t, int64_t> _batteryCurrent = Samples<int32_t, int64_t>(10);
#pragma endregion

void InterceptMessage(SCanMessage* message)
{
    //LogMessage(*message);

    switch (message->id)
    {
    case 0x100:
    {
        if (message->data[0] == 0x05
            && message->data[1] == 0x2E
            && message->data[2] == 0x02
            && message->data[4] == 0x00
            && message->data[5] == 0x00
            && message->data[6] == 0x00
            && message->data[7] == 0x00)
        {
            LOGD(nameof(CAN::BusMaster), "Received request for string: %x", message->data[3]);
            if (_stringRequestBuffer != nullptr)
            {
                LOGW(nameof(CAN::BusMaster), "New string request before previous request was processed.");
                delete[] _stringRequestBuffer;
            }
            _stringRequestType = (EStringType)message->data[3];
            _stringRequestBuffer = new char[20];
        }
        break;
    }
    case 0x101:
    {
        if (message->data[0] == 0x10
            && message->data[2] == 0x62
            && message->data[3] == 0x02)
        {
            //String response.
            LOGD(nameof(CAN::BusMaster), "Received string response for %x.", message->data[4]);
            if (_stringRequestBuffer == nullptr)
            {
                LOGW(nameof(CAN::BusMaster), "String response received before a request was sent.");
                break;
            }

            for (size_t i = 5; i < 8; i++)
                _stringRequestBuffer[_stringRequestBufferIndex++] = message->data[i];
        }
        else if (message->data[0] == 0x21 || message->data[1] == 0x22)
        {
            //String response continued.
            LOGD(nameof(CAN::BusMaster), "Continued response for %x.", _stringRequestType);
            if (_stringRequestBuffer == nullptr)
            {
                LOGW(nameof(CAN::BusMaster), "String response continued before a request was sent.");
                break;
            }

            for (size_t i = 1; i < 8; i++)
                _stringRequestBuffer[_stringRequestBufferIndex++] = message->data[i];
        }
        else if (message->data[0] == 0x23)
        {
            //String response end.
            LOGD(nameof(CAN::BusMaster), "String response end for %x.", _stringRequestType);
            if (_stringRequestBuffer == nullptr)
            {
                LOGW(nameof(CAN::BusMaster), "String response end before a request was sent.");
                break;
            }
            for (size_t i = 1; i < 4; i++)
                _stringRequestBuffer[_stringRequestBufferIndex++] = message->data[i];
            LOGD(nameof(CAN::BusMaster), "String response for %x: %s", _stringRequestType, _stringRequestBuffer);
        }
        else if (message->data[0] == 0x05
            && message->data[1] == 0x62
            && message->data[2] == 0x02
            && message->data[3] == 0x06
            && message->data[6] == 0xE0
            && message->data[7] == 0xAA)
        {
            _wheelCircumference = message->data[4] | message->data[5] << 8;
            LOGD(nameof(CAN::BusMaster), "Received wheel circumference: %u", _wheelCircumference);
        }
        break;
    }
    case 0x201:
    {
        //D1 and D2 combined contain the speed of the bike in km/h * 100 (little-endian).
        //Example: EA, 01 -> 01EA -> 490 -> 4.9km/h.
        //We won't work in decimals.
        _bikeSpeedBuffer.AddSample(message->data[0] | message->data[1] << 8);
        break;
    }
    case 0x300:
    {
        _walkMode = message->data[1] == 0xA5;
        _easeSetting = message->data[4];
        _powerSetting = message->data[6];
        break;
    }
    case 0x401:
    {
        _batteryVoltage.AddSample(message->data[0] | message->data[1] << 8);

        //D5, D6, D7 and D8 combined contain the battery current in mA (little-endian with two's complement).
        //Example 1: 46, 00, 00, 00 -> 00 000046 -> 70mA.
        //Example 2: 5B, F0, FF, FF -> FF FFF05B -> -4005mA.
        _batteryCurrent.AddSample(message->data[4] | message->data[5] << 8 | message->data[6] << 16 | message->data[7] << 24);
        break;
    }
    default:
        break;
    }
}
