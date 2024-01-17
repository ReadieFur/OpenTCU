#include <Arduino.h>
#include <WiFi.h>
#include <driver/gpio.h>
#include <driver/twai.h>
// #include <mcp_can.h>
#include "Logger.hpp"
// #include "CAN.hpp"
#include <utility>
#include <mcp2515.h>

#define SPI_BUS HSPI
#define SCK_PIN GPIO_NUM_0
#define MISO_PIN GPIO_NUM_1
#define MOSI_PIN GPIO_NUM_2
#define CAN_SPEED CAN_250KBPS
// #define CAN1_SPI_BUS FSPI
#define CAN1_SS_PIN GPIO_NUM_3
// #define CAN2_SPI_BUS HSPI
#define CAN2_SS_PIN GPIO_NUM_4

#define CAN_WRITE_MAX_DELAY (1000 / 250000 / portTICK_PERIOD_MS) / 3

// CAN can1(SPI_BUS, SCK_PIN, MISO_PIN, MOSI_PIN, CAN1_SS_PIN, CAN_SPEED);
// CAN can2(SPI_BUS, SCK_PIN, MISO_PIN, MOSI_PIN, CAN2_SS_PIN, CAN_SPEED);

// void RelayTask(void* parameter)
// {
// 	std::pair<CAN*, CAN*>* pair = static_cast<std::pair<CAN*, CAN*>*>(parameter);
// 	CAN* canA = pair->first;
// 	CAN* canB = pair->second;
// 	delete pair;

// 	while (true)
// 	{
// 		twai_message_t message;
// 		if (uint8_t err = canA->Read(&message, portMAX_DELAY) != CAN_OK)
// 		{
// 			Logger::Log("canA receive failed: %#x\n", err);
// 			return;
// 		}

// 		//Relay the message to the other CAN bus.
// 		if (uint8_t err = canB->Write(&message, CAN_WRITE_MAX_DELAY) != CAN_OK)
// 		{
// 			Logger::Log("canB send failed: %#x\n", err);
// 			return;
// 		}
// 	}
// }

// void setup()
// {
// 	// #pragma region CAN setup
// 	// //Create high priority tasks to handle CAN relays.
// 	// xTaskCreate(RelayTask, "CAN1Task", 1024, new std::pair<CAN*, CAN*>(&can1, &can2), 5, NULL);
// 	// xTaskCreate(RelayTask, "CAN2Task", 1024, new std::pair<CAN*, CAN*>(&can2, &can1), 5, NULL);
// 	// #pragma endregion
	
// 	// Logger::Log("Primary setup complete.\n");

// 	Logger::Begin();

// 	#pragma region WiFi setup
// 	#ifdef MASTER
// 	WiFi.mode(WIFI_AP_STA);
// 	WiFi.softAP("ESP32", "123456789");
// 	Logger::Log("IP address: %s\n", WiFi.softAPIP().toString().c_str());
// 	#else
// 	WiFi.mode(WIFI_STA);
// 	#endif
// 	Logger::Log("Mac address: %s\n", WiFi.macAddress().c_str());
// 	#pragma endregion

// 	Logger::Log("Secondary setup complete.\n");
// }

void loop()
{
	vTaskDelete(NULL);
}

//https://how2electronics.com/interfacing-mcp2515-can-bus-module-with-arduino/
void setup()
{
	printf("Hello world!\n");
	delay(5000);

	printf("Testing SPI CAN...\n");
	MCP2515 mcp2515(SS);
	MCP2515::ERROR err;
	err = mcp2515.reset();
	if (err != MCP2515::ERROR_OK)
	{
		printf("MCP2515 reset failed: %#x\n", err);
		return;
	}
	err = mcp2515.setBitrate(CAN_250KBPS);
	if (err != MCP2515::ERROR_OK)
	{
		printf("MCP2515 setBitrate failed: %#x\n", err);
		return;
	}
	err = mcp2515.setNormalMode();
	if (err != MCP2515::ERROR_OK)
	{
		printf("MCP2515 setNormalMode failed: %#x\n", err);
		return;
	}
}
