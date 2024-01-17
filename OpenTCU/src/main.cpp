#include <Arduino.h>
#include <WiFi.h>
#include <driver/gpio.h>
#include <driver/twai.h>
#include "Logger.hpp"
#include "TwoChip.hpp"
#include "CAN.hpp"

#define CAN_RX_PIN GPIO_NUM_1
#define CAN_TX_PIN GPIO_NUM_2

// #define TEST

#define CAN_FREQUENCY 250000
#define CAN_INTERVAL 1000 / CAN_FREQUENCY
#ifdef TEST
#define MAX_DELAY portMAX_DELAY
#else
#define MAX_DELAY (CAN_INTERVAL / portTICK_PERIOD_MS) / 3
#endif

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

void SecondarySetupTask(void* parameter)
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

	Logger::Log("Secondary setup complete.\n");

	vTaskDelete(NULL);
}

void setup()
{
	#pragma region TwoChip setup
	uint8_t peerMac[] =
	{
	#ifdef MASTER
		SLAVE_MAC
	#else
		MASTER_MAC
	#endif
	};

	// for (uint8_t i = 0; i < 6; i++)
	// {
	// 	if (peerMac[i] != 0x00)
	// 		break;

	// 	if (i == 5)
	// 	{
	// 		Logger::Log("Invalid peer MAC address.");
	// 		return;
	// 	}
	// }

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
		TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL),
		TWAI_TIMING_CONFIG_250KBITS(),
		TWAI_FILTER_CONFIG_ACCEPT_ALL()
	);
	#endif
	
	Logger::Log("Primary setup complete.\n");

	//Create a second low-priority task to handle the rest of the setup as the CAN transceiver MUST take priority.
	xTaskCreate(SecondarySetupTask, "SecondarySetupTask", 4096, NULL, 1, NULL);
}

void loop()
{
	// delay(MAX_DELAY);

	#ifdef USE_SERIAL_CAN
	#else
	#ifdef TEST
	twai_status_info_t statusInfo;
	if (esp_err_t err = can->GetStatus(&statusInfo) != ESP_OK)
	{
		Logger::Log("twai_get_status_info failed: %#x\n", err);
	}
	else
	{
		Logger::Log("State: %#x\nQueue (TX/RX): %d/%d\nErrors: %d/%d\n", statusInfo.state, statusInfo.msgs_to_tx, statusInfo.msgs_to_rx, statusInfo.tx_error_counter, statusInfo.rx_error_counter);
	}

	#ifdef MASTER
	Logger::Log("Waiting for message...\n");
	twai_message_t message;
	if (esp_err_t err = can->Read(&message, MAX_DELAY) != ESP_OK)
	{
		Logger::Log("twai_receive failed: %#x\n", err);
	}
	else
	{
		if (message.identifier == 1)
			Logger::Log("Received test message.\n");
		else
			Logger::Log("Received message: %#x\n", message.identifier);
	}
	#else
	Logger::Log("Sending message...\n");
	twai_message_t testMessage;
	testMessage.identifier = 0x123;
	testMessage.flags = TWAI_MSG_FLAG_EXTD;
	testMessage.data_length_code = 1;
	testMessage.data[0] = 0x01;
	if (esp_err_t err = can->Send(testMessage, MAX_DELAY) != ESP_OK)
		Logger::Log("twai_transmit test failed: %#x\n", err);
	else
		Logger::Log("Sent test message.\n");
	#endif
	delay(1000);
	#else
	twai_message_t message;
	if (esp_err_t err = can->Read(&message, portMAX_DELAY) != ESP_OK)
	{
		Logger::Log("twai_receive failed: %#x\n", err);
		return;
	}
	uint8_t dataSize;
	uint8_t* data = TWAIMessageToArray(&message, &dataSize);
	TwoChip::Send(data, dataSize);
	delete[] data;
	#endif
	#endif
}
