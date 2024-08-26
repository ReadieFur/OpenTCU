#include <Arduino.h>
#include <iostream>
#include <cmath>

#define WHEEL_CIRCUMFERENCE 2160 //Wheel circumference in mm (configured for the stock Turbo Levo 27.5" wheels).
#define SPEED_MULTIPLIER 1.5f //The amount to decrease the real bike speed by.
#define ACTIVATION_SPEED 20.0f //The speed to activate the modifier at in km/h.
#define DEACTIVATION_SPEED 40.0f //The speed to deactivate the modifier at in km/h.

#define REED_POSITIVE GPIO_NUM_0
#define REED_NEGATIVE GPIO_NUM_1
#define RELAY_BASE GPIO_NUM_2
#define TIMEOUT 3 * 1000000UL //Timeout of x seconds (in microseconds).
#define MIN_RPM static_cast<int>((ACTIVATION_SPEED * 1000 / 60) / (WHEEL_CIRCUMFERENCE / 1000)) //Minimum RPM for applying the multiplier (round down). Convert speed to m/min, convert WHEEL_CIRCUMFERENCE to meters, divide the two for RPM.
#define MAX_RPM static_cast<int>((DEACTIVATION_SPEED * 1000 / 60) / (WHEEL_CIRCUMFERENCE / 1000) + 1) //Maximum RPM for applying the multiplier (round up, +1 rounds up with static_cast<int> truncating any decimal, assuming there is a decimal).

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
	ulong highDuration = pulseIn(REED_NEGATIVE, HIGH, TIMEOUT);
	
	//Measure the duration of the LOW signal.
	ulong lowDuration = pulseIn(REED_NEGATIVE, LOW, TIMEOUT);

	//If a timeout occurs, pulseIn() returns 0.
	if (highDuration > 0 && lowDuration > 0)
	{
		//Calculate the total cycle duration in microseconds.
		ulong totalCycleDuration = highDuration + lowDuration;
		//Calculate RPM.
		float totalCycleDurationSeconds = totalCycleDuration / 1000000.0; //Convert to seconds.
		float rpm = 60.0 / totalCycleDurationSeconds; //RPM = 60 / (Total Cycle Duration in seconds).
		//Determine if the RPM is within the specified range.
		bool applyMultiplier = (rpm >= MIN_RPM && rpm <= MAX_RPM);

		//Calculate the modified durations.
		ulong modifiedHighDuration = applyMultiplier ? highDuration * SPEED_MULTIPLIER : highDuration;
		ulong modifiedLowDuration = applyMultiplier ? lowDuration * SPEED_MULTIPLIER : lowDuration;

		//Relay the signal with the modified timing.
		digitalWrite(RELAY_BASE, HIGH);
		delayMicroseconds(modifiedHighDuration);
		
		digitalWrite(RELAY_BASE, LOW);
		delayMicroseconds(modifiedLowDuration);

		ulong now = millis();
		if (now - lastLogTime >= 1000UL)
		{
			//Log the accumulated on and off times in milliseconds.
			std::cout
				<< "On Duration: " << (highDuration / 1000.0) << "ms"
				<< ", Off Duration: " << (lowDuration / 1000.0) << "ms"
				<< ", RPM: " << rpm
				<< ", Apply Multiplier: " << applyMultiplier
				<< ", Fake On Duration: " << (modifiedHighDuration / 1000.0) << "ms"
				<< ", Fake Off Duration: " << (modifiedLowDuration / 1000.0) << "ms"
				<< ", Fake RPM: " << (applyMultiplier ? rpm / SPEED_MULTIPLIER : rpm)
				<< std::endl;
			
			lastLogTime = now;
		}
	}
	else
	{
		ulong now = millis();
		if (now - lastLogTime >= 1000UL)
		{
			std::cout << "Idle " << (highDuration > 0 ? highDuration / 1000 : highDuration)
				<< " " << (lowDuration > 0 ? lowDuration / 1000 : lowDuration) << std::endl;
			lastLogTime = now;
		}
	}
}
