// #pragma once

// #include <driver/gpio.h>
// #include "Logger.hpp"
// #include <mcp_can.h>
// #include <SPI.h>
// #include <driver/twai.h>

// class CAN
// {
// private:
//     SPIClass* _spi;
//     MCP_CAN* _can;
// public:
//     CAN(
//         const uint8_t spiBus,
//         const gpio_num_t sckPin,
//         const gpio_num_t misoPin,
//         const gpio_num_t mosiPin,
//         const gpio_num_t ssPin,
//         const uint8_t canSpeed)
//     {
//         _spi = new SPIClass(spiBus);
//         _spi->begin(sckPin, misoPin, mosiPin, ssPin);
        
//         _can = new MCP_CAN(_spi, ssPin);
//         _can->begin(MCP_ANY, canSpeed, MCP_16MHZ);
//         _can->setMode(MCP_NORMAL);
//     }

//     ~CAN()
//     {
//         delete _can;
//         delete _spi;
//     }

//     uint8_t Read(twai_message_t* message, const TickType_t ticksToWait)
//     {
//         uint8_t result = CAN_FAIL;
//         uint8_t buffer[8];
//         uint8_t length = 0;
//         uint32_t id = 0;
//         uint8_t extd = 0;
//         if (result = _can->readMsgBuf(&id, &extd, &length, buffer) == CAN_OK)
//         {
//             message->identifier = id;
//             message->data_length_code = length;
//             message->extd = extd == CAN_EXTID ? 1 : 0;
//             memcpy(message->data, buffer, length);
//         }
//         return result;
//     }

//     uint8_t Write(const twai_message_t* message, const TickType_t ticksToWait)
//     {
//         return _can->sendMsgBuf(
//             message->identifier,
//             message->extd == 1 ? CAN_EXTID : CAN_STDID,
//             message->data_length_code,
//             const_cast<uint8_t*>(message->data)
//         );
//     }
// };
