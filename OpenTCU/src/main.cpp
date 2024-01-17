#include <OneWire.h> 
#include <DallasTemperature.h>
#include <BluetoothSerial.h>
#include <SPI.h>
#include "mcp_can.h"
//------CAN BUS setting ---------------------------------------------------------
#define CAN0_INT GPIO_NUM_10 // Set INT to pin 50
const int SPI_CS_PIN = GPIO_NUM_7;
MCP_CAN CAN0(SPI_CS_PIN);

void setup()
{
}

void loop()
{
}
