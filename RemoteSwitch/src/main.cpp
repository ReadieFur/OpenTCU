#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerialLite.h>
#include <gpio.h>

#define POWER_PIN D8
#define POWER_CHECK_PIN D2
#define POWER_SWITCH_PIN D7
#define CAN_SWITCH_PIN D6
#define LED_PIN D4
#define TEST_PIN D3

#define ON LOW
#define OFF HIGH

AsyncWebServer server(80);

void Log(const char* format, ...)
{
	char buffer[64];

	//Use variadic arguments to format the string.
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	//Print the string.
	Serial.print(buffer);
	WebSerial.print(buffer);
}

void Blip(int count = 1)
{
	for (int i = 0; i < count; i++)
	{
		if (i > 0)
			delay(100);
		digitalWrite(LED_PIN, ON);
		delay(100);
		digitalWrite(LED_PIN, OFF);
	}
}

void PowerOff()
{
	digitalWrite(POWER_PIN, OFF);
	// digitalWrite(CAN_SWITCH_PIN, OFF);
	digitalWrite(POWER_SWITCH_PIN, OFF);
	digitalWrite(LED_PIN, ON); //Invert for signaling that the power is off.
}

void PowerOn()
{
	digitalWrite(POWER_PIN, ON);
	// digitalWrite(CAN_SWITCH_PIN, ON);
	digitalWrite(LED_PIN, OFF);
}

void EnableDirectCAN()
{
	digitalWrite(CAN_SWITCH_PIN, ON);
	// Blip();
}

void DisableDirectCAN()
{
	digitalWrite(CAN_SWITCH_PIN, ON);
	// Blip(2);
}

void PinStatus()
{
	Log("Power Switch: %d\n", digitalRead(POWER_PIN) == ON);
	Log("Power Switch: %d\n", digitalRead(POWER_SWITCH_PIN) == ON);
	Log("CAN Switch: %d\n", digitalRead(CAN_SWITCH_PIN) == ON);
	Log("Power Check: %d\n", digitalRead(POWER_CHECK_PIN) == ON);
}

void PowerSwitch()
{
	//Turn on then off.
	digitalWrite(POWER_SWITCH_PIN, ON);
	delay(100);
	digitalWrite(POWER_SWITCH_PIN, OFF);
	Log("Toggled power switch.\n");
}

void TestPin()
{
	int newState = digitalRead(TEST_PIN) == ON ? OFF : ON;
	Log("Test pin: %d\n", newState);
	digitalWrite(TEST_PIN, newState);
	digitalWrite(LED_PIN, newState);
}

void ReceiveMessage(const uint8_t* data, uint8_t dataSize)
{
	if (dataSize == 0)
		return;

	switch (data[0])
	{
	case 0x30: //0
		PowerOff();
		Log("Power: OFF\n");
		break;
	case 0x31: //1
		PowerOn();
		Log("Power: ON\n");
		break;
	case 0x32: //2
		EnableDirectCAN();
		Log("Direct CAN: ON\n");
		break;
	case 0x33: //3
		DisableDirectCAN();
		Log("Direct CAN: OFF\n");
		break;
	case 0x34: //4
		PowerSwitch();
		break;
	case 0x35: //5
		PinStatus();
		break;
	case 0x36: //6
		TestPin();
		break;
	case 0x37: //7
		Blip();
		break;
	case 0x38: //8
	case 0x39: //9
	default:
		break;
	}
}

void setup()
{
	Serial.begin(9600);

	pinMode(POWER_PIN, OUTPUT);
	pinMode(POWER_SWITCH_PIN, OUTPUT);
	pinMode(CAN_SWITCH_PIN, OUTPUT);
	pinMode(POWER_CHECK_PIN, INPUT_PULLUP);
	pinMode(LED_PIN, OUTPUT);
	digitalWrite(LED_PIN, OFF);
	PowerOff();
	EnableDirectCAN();

	pinMode(TEST_PIN, OUTPUT);
	digitalWrite(TEST_PIN, ON);

	//Allow time for Serial to connect. Then print the pin status.
	delay(1000);
	PinStatus();

	WiFi.mode(WIFI_STA);
	IPAddress staticIP(192, 168, 0, 156);
	IPAddress gateway(192, 168, 1, 254);
	IPAddress subnet(255, 255, 254, 0);
	IPAddress dns(192, 168, 1, 254);
	if (!WiFi.config(staticIP, gateway, subnet, dns))
	{
		Log("STA configuration failed.\n");
		//Reboot.
		ESP.restart();
	}

	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	if (WiFi.waitForConnectResult() != WL_CONNECTED)
	{
		Log("WiFi connection failed.\n");
		//Reboot.
		ESP.restart();
	}
	Log((WiFi.localIP().toString() + "\n").c_str());
	WebSerial.begin(&server);
	WebSerial.onMessage(ReceiveMessage);
	server.begin();
}

void loop()
{
	delay(250);

	//If the power is off and we have the power state set to on, call the power off function.
	if (digitalRead(POWER_CHECK_PIN) == OFF && digitalRead(POWER_PIN) == ON)
	{
		PowerOff();
		Log("Power: OFF (External)\n");
	}
	else if (digitalRead(POWER_CHECK_PIN) == ON && digitalRead(POWER_PIN) == OFF)
	{
		PowerOn();
		Log("Power: ON (External)\n");
	}

	//If we loose connection to the WiFi, write all pins ON and reboot.
	if (WiFi.status() != WL_CONNECTED)
	{
		PowerOff();
		Log("Power: OFF (WiFi)\n");
		//Reboot.
		ESP.restart();
	}

	// delay(2500);
	// int newState = digitalRead(TEST_PIN) == ON ? OFF : ON;
	// Serial.printf("Test pin: %d\n", newState);
	// digitalWrite(TEST_PIN, newState);
	// digitalWrite(LED_PIN, newState);
}
