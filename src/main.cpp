#include <Arduino.h>
#include <iostream>

#define REED_POSITIVE GPIO_NUM_0
#define REED_NEGATIVE GPIO_NUM_1
#define RELAY_BASE GPIO_NUM_2

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
	int enabled = digitalRead(REED_NEGATIVE);
	digitalWrite(RELAY_BASE, enabled);
	#if true
	std::cout << "Reed:" << enabled << std::endl;
	#endif
	delay(10); //Prevent the CPU from running too fast (adds extra calls).
}
