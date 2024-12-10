#pragma once

#include "SCanMessage.h"
#include <stdio.h>

#define nameof(x) #x
#define LOG(level, tag, ...) printf("[" level "] " tag ": "); printf(__VA_ARGS__); printf("\n")
#define LOGE(tag, ...) LOG("E", tag, __VA_ARGS__)
#define LOGW(tag, ...) LOG("W", tag, __VA_ARGS__)
#define LOGI(tag, ...) LOG("I", tag, __VA_ARGS__)
#define LOGD(tag, ...) LOG("D", tag, __VA_ARGS__)
#define LOGV(tag, ...) LOG("V", tag, __VA_ARGS__)

void LogMessage(SCanMessage message)
{
    switch (message.length)
    {
    case 0:
        LOGI(nameof(CAN::Logger), "%u,%x,%u,%u,%u",
            message.bus,
            message.id,
            message.isExtended,
            message.isRemote,
            message.length);
        break;
    case 1:
        LOGI(nameof(CAN::Logger), "%u,%x,%u,%u,%u,%02X",
            message.bus,
            message.id,
            message.isExtended,
            message.isRemote,
            message.length,
            message.data[0]);
        break;
    case 2:
        LOGI(nameof(CAN::Logger), "%u,%x,%u,%u,%u,%02X,%02X",
            message.bus,
            message.id,
            message.isExtended,
            message.isRemote,
            message.length,
            message.data[0],
            message.data[1]);
        break;
    case 3:
        LOGI(nameof(CAN::Logger), "%u,%x,%u,%u,%u,%02X,%02X,%02X",
            message.bus,
            message.id,
            message.isExtended,
            message.isRemote,
            message.length,
            message.data[0],
            message.data[1],
            message.data[2]);
        break;
    case 4:
        LOGI(nameof(CAN::Logger), "%u,%x,%u,%u,%u,%02X,%02X,%02X,%02X",
            message.bus,
            message.id,
            message.isExtended,
            message.isRemote,
            message.length,
            message.data[0],
            message.data[1],
            message.data[2],
            message.data[3]);
        break;
    case 5:
        LOGI(nameof(CAN::Logger), "%u,%x,%u,%u,%u,%02X,%02X,%02X,%02X,%02X",
            message.bus,
            message.id,
            message.isExtended,
            message.isRemote,
            message.length,
            message.data[0],
            message.data[1],
            message.data[2],
            message.data[3],
            message.data[4]);
        break;
    case 6:
        LOGI(nameof(CAN::Logger), "%u,%x,%u,%u,%u,%02X,%02X,%02X,%02X,%02X,%02X",
            message.bus,
            message.id,
            message.isExtended,
            message.isRemote,
            message.length,
            message.data[0],
            message.data[1],
            message.data[2],
            message.data[3],
            message.data[4],
            message.data[5]);
        break;
    case 7:
        LOGI(nameof(CAN::Logger), "%u,%x,%u,%u,%u,%02X,%02X,%02X,%02X,%02X,%02X,%02X",
            message.bus,
            message.id,
            message.isExtended,
            message.isRemote,
            message.length,
            message.data[0],
            message.data[1],
            message.data[2],
            message.data[3],
            message.data[4],
            message.data[5],
            message.data[6]);
        break;
    default:
        LOGI(nameof(CAN::Logger), "%u,%x,%u,%u,%u,%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X",
            message.bus,
            message.id,
            message.isExtended,
            message.isRemote,
            message.length,
            message.data[0],
            message.data[1],
            message.data[2],
            message.data[3],
            message.data[4],
            message.data[5],
            message.data[6],
            message.data[7]);
        break;
    }
}
