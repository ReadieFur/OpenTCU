#include <Arduino.h>
#include <WiFi.h>
#include <driver/gpio.h>
#include <driver/twai.h>
#include "Logger.hpp"
#include "TwoChip.hpp"
#include "CAN.hpp"

#define CAN_FREQUENCY 250000
#define CAN_INTERVAL 1000 / CAN_FREQUENCY
#define MAX_DELAY (CAN_INTERVAL / portTICK_PERIOD_MS) / 3

CAN* can;

uint8_t* TWAIMessageToArray(const twai_message_t* message, uint8_t* dataSize)
{
	*dataSize = sizeof(twai_message_t);
	uint8_t* data = new uint8_t[*dataSize];
	memcpy(data, message, *dataSize);
	return data;
}

twai_message_t* ArrayToTWAIMessage(const uint8_t* data, uint8_t dataSize)
{
	twai_message_t* message = new twai_message_t();
	memcpy(message, data, dataSize);
	return message;
}

void TwoChip_OnReceive(const uint8_t* data, uint8_t dataSize)
{
	#ifdef USE_SERIAL_CAN
	#else
	twai_message_t* message = ArrayToTWAIMessage(data, dataSize);
	if (esp_err_t err = can->Send(*message, MAX_DELAY) != ESP_OK)
	{
		Logger::Log("twai_transmit failed: %#x\n", err);
		return;
	}
	delete message;
	#endif
}

void setup()
{
	Logger::Begin();

	#pragma region WiFi setup
	#ifdef MASTER
	WiFi.mode(WIFI_AP_STA);
	WiFi.softAP("ESP32", "123456789");
	Logger::Log("IP address: %s\n", WiFi.softAPIP().toString().c_str());
	#else
	WiFi.mode(WIFI_STA);
	#endif
	Logger::Log("Mac address: %s\n", WiFi.macAddress().c_str());
	#pragma endregion

	#pragma region TwoChip setup
	uint8_t peerMac[] =
	{
	#ifdef MASTER
		SLAVE_MAC
	#else
		MASTER_MAC
	#endif
	};

	for (uint8_t i = 0; i < 6; i++)
	{
		if (peerMac[i] != 0x00)
			break;

		if (i == 5)
		{
			Logger::Log("Invalid peer MAC address.");
			return;
		}
	}

	if (esp_err_t err = TwoChip::Begin(peerMac) != ESP_OK)
	{
		Logger::Log("TwoChip setup failed: %#x\n", err);
		return;
	}

	TwoChip::receiveCallback = TwoChip_OnReceive;
	#pragma endregion

	#pragma region CAN setup
	#ifdef USE_SERIAL_CAN
	#else
	can = new CAN(
		TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_1, GPIO_NUM_2, TWAI_MODE_NORMAL),
		TWAI_TIMING_CONFIG_250KBITS(),
		TWAI_FILTER_CONFIG_ACCEPT_ALL()
	);
	#endif

	Logger::Log("Setup complete.\n");
}

void loop()
{
	delay(MAX_DELAY);

	#ifdef USE_SERIAL_CAN
	#else
	twai_message_t message;
	if (esp_err_t err = can->Read(&message, MAX_DELAY) != ESP_OK)
	{
		Logger::Log("twai_receive failed: %#x\n", err);
		return;
	}
	uint8_t dataSize;
	uint8_t* data = TWAIMessageToArray(&message, &dataSize);
	TwoChip::Send(data, dataSize);
	delete[] data;
	#endif
}
