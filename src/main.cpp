#include <Arduino.h>
#include <iostream>

#define WHEEL_CIRCUMFERENCE 2160 //Wheel circumference in mm (configured for the stock Turbo Levo 27.5" wheels).
#define SPEED_MULTIPLIER 1.5f //The amount to decrease the real bike speed by.
#define ACTIVATION_SPEED 20.0f //The speed to activate the modifier at in km/h.
#define DEACTIVATION_SPEED 40.0f //The speed to deactivate the modifier at in km/h.

#define REED_POSITIVE GPIO_NUM_0
#define REED_NEGATIVE GPIO_NUM_1
#define RELAY_BASE GPIO_NUM_2
#define TIMEOUT 1000000UL //Timeout of 1 second (in microseconds).
#define MIN_RPM (ACTIVATION_SPEED * 1000 / 60) / (WHEEL_CIRCUMFERENCE / 1000) //Minimum RPM for applying the multiplier. Convert speed to m/min, convert WHEEL_CIRCUMFERENCE to meters, divide the two for RPM.
#define MAX_RPM (DEACTIVATION_SPEED * 1000 / 60) / (WHEEL_CIRCUMFERENCE / 1000) //Maximum RPM for applying the multiplier.

ulong lastLogTime = 0;

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
	//Measure the duration of the HIGH signal.
	ulong highDuration = pulseIn(GPIO_NUM_1, HIGH, TIMEOUT);
	
	//Measure the duration of the LOW signal.
	ulong lowDuration = pulseIn(GPIO_NUM_1, LOW, TIMEOUT);

	//If a timeout occurs, pulseIn() returns 0
	if (highDuration > 0 && lowDuration > 0)
	{
		//Calculate the total cycle duration in microseconds
		ulong totalCycleDuration = highDuration + lowDuration;
		float rpm = 60.0 / (totalCycleDuration / 1000000.0); //RPM = 60 / (Total Cycle Duration in seconds).
		//Determine if the RPM is within the specified range
		bool applyMultiplier = (rpm >= MIN_RPM && rpm <= MAX_RPM);

		//Calculate the modified durations.
		ulong modifiedHighDuration = highDuration * SPEED_MULTIPLIER;
		ulong modifiedLowDuration = lowDuration * SPEED_MULTIPLIER;

		//Relay the signal with the modified timing.
		if (highDuration > 0)
		{
			digitalWrite(RELAY_BASE, HIGH);
			delayMicroseconds(modifiedHighDuration);
		}
		
		if (lowDuration > 0)
		{
			digitalWrite(RELAY_BASE, LOW);
			delayMicroseconds(modifiedLowDuration);
		}

		ulong now = millis();
		if (now - lastLogTime >= 1000UL)
		{
			//Log the accumulated on and off times in milliseconds.
			std::cout
				<< "On Duration: " << (highDuration / 1000.0) << "ms"
				<< ", Off Duration: " << (lowDuration / 1000.0) << "ms"
				<< ", Fake On Duration: " << (modifiedHighDuration / 1000.0) << "ms"
				<< ", Off Duration: " << (modifiedLowDuration / 1000.0) << "ms"
				<< std::endl;
			
			lastLogTime = now;
		}
	}

	vTaskDelay(1);
}
