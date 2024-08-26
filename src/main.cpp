#include <Arduino.h>
#include <iostream>
#include <cmath>

#define REED_POSITIVE GPIO_NUM_0
#define REED_NEGATIVE GPIO_NUM_1
#define RELAY_BASE GPIO_NUM_2

int lastState = LOW;
ulong cycles = 1; //Start on 1 to not skip the first real input.

void setup()
{
	Serial.begin(115200);

	pinMode(REED_POSITIVE, OUTPUT);
	pinMode(REED_NEGATIVE, INPUT_PULLDOWN);
	pinMode(RELAY_BASE, OUTPUT);
	pinMode(GPIO_NUM_3, INPUT_PULLDOWN);
	digitalWrite(REED_POSITIVE, HIGH);
}

void loop()
{
	int state = digitalRead(REED_NEGATIVE);

	//Increment the counter on every rising edge.
	if (lastState == LOW && state == HIGH)
	{
		cycles++;

		std::cout << (cycles % 2 == 0 ? "Relay" : "Skip") << std::endl;
	}
	else if (lastState == HIGH && state == LOW)
	{
		std::cout << "Low" << std::endl;
	}

	lastState = state;

	int relayState;
	if (state == HIGH)
		//Only enable the sensor on every other HIGH signal.
		relayState = cycles % 2 == 0 ? HIGH : LOW;
	else
		//Send all LOW signals.
		relayState = LOW;

	digitalWrite(RELAY_BASE, relayState);

	vTaskDelay(0);
}
