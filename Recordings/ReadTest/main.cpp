#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <vector>
#include "Samples.hpp"
#include "EStringType.h"

#define nameof(x) #x
#define LOG(level, tag, ...) printf("[" level "] " tag ": "); printf(__VA_ARGS__); printf("\n")
#define LOGE(tag, ...) LOG("E", tag, __VA_ARGS__)
#define LOGW(tag, ...) LOG("W", tag, __VA_ARGS__)
#define LOGI(tag, ...) LOG("I", tag, __VA_ARGS__)
#define LOGD(tag, ...) LOG("D", tag, __VA_ARGS__)
#define LOGV(tag, ...) LOG("V", tag, __VA_ARGS__)

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

struct SCanMessage
{
    bool bus;
    uint32_t id;
    uint8_t data[8];
    uint8_t length;
    bool isExtended;
    bool isRemote;
};

void InterceptMessage(SCanMessage* message)
{
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

bool ParseLine(std::string line, SCanMessage* message)
{
    //Example input:
    //10:52:48.266 CAN::Logger:10598,0,301,0,0,3,B5,0C,00

	//Only process lines that contain "CAN::Logger".
	if (line.find("CAN::Logger") == std::string::npos)
		return false;

    //Split the line by commas.
	size_t pos = 0;
	std::vector<std::string> tokens;
	while ((pos = line.find(',')) != std::string::npos)
	{
		tokens.push_back(line.substr(0, pos));
		line.erase(0, pos + 1);
	}
	tokens.push_back(line); //Add the remainder of the line to the tokens list.

	//A valid message should have at least 5 tokens.
	if (tokens.size() < 5)
		return false;

	message->bus = std::stoi(tokens[1]) == 0 ? false : true;
	message->id = std::stoi(tokens[2], nullptr, 16);
    message->isExtended = std::stoi(tokens[3]) == 0 ? false : true;
	message->isRemote = std::stoi(tokens[4]) == 0 ? false : true;
    message->length = std::stoi(tokens[5]);
	for (int i = 0; i < message->length; i++)
		message->data[i] = std::stoi(tokens[6 + i], nullptr, 16);

	return true;
}

int main()
{
    std::string filePath = "C:\\Users\\Chloe\\Documents\\GitHub\\ReadieFur\\OpenTCU\\Recordings\\valuable_recordings\\serial_20241205_105247.txt";

    //Prompt the user for the file path.
    if (filePath.empty())
    {
        std::cout << "Enter the path to the .txt file: ";
        std::getline(std::cin, filePath);
    }

    //I use the windows copy file path feature which wraps a file in quotation marks so remove these if they are present.
	if (filePath.front() == '\"' && filePath.back() == '\"')
		filePath = filePath.substr(1, filePath.size() - 2);

    //Check if the file exists.
    if (!std::filesystem::exists(filePath))
    {
        std::cerr << "File not found. Please check the path and try again." << std::endl;
        return 1;
    }

    //Open the file for reading.
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        std::cerr << "Failed to open the file!" << std::endl;
        return 2;
    }

    std::string line;
    while (std::getline(file, line))
    {
        SCanMessage message;
        if (!ParseLine(line, &message))
            continue;
        InterceptMessage(&message);
    }

    file.close();

    return 0;
}
