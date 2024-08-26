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
#define TIMEOUT 3 * 1000 //Timeout of x seconds (in milliseconds).
#define MIN_RPM static_cast<int>((ACTIVATION_SPEED * 1000 / 60) / (WHEEL_CIRCUMFERENCE / 1000)) //Minimum RPM for applying the multiplier (round down). Convert speed to m/min, convert WHEEL_CIRCUMFERENCE to meters, divide the two for RPM.
#define MAX_RPM static_cast<int>((DEACTIVATION_SPEED * 1000 / 60) / (WHEEL_CIRCUMFERENCE / 1000) + 1) //Maximum RPM for applying the multiplier (round up, +1 rounds up with static_cast<int> truncating any decimal, assuming there is a decimal).

ulong lastLogTime = 0;
volatile int lastState = LOW;
volatile ulong onTime = 0;
volatile ulong offTime = 0;
volatile ulong onDuration = 0;
volatile ulong offDurationTmp = 0;
volatile ulong offDuration = 0;
volatile ulong lastDurationUpdate = 0;
void IRAM_ATTR ReadISR()
{
	ulong now = millis();
	int state = digitalRead(REED_NEGATIVE);

	if (lastState == LOW && state == HIGH)
	{
		offDurationTmp = now - offTime;
		onTime = now;
	}
	else if (lastState == HIGH && state == LOW)
	{
		onDuration = now - onTime;
		offDuration = offDurationTmp;
		lastDurationUpdate = now;
		offTime = now;
	}

	lastState = state;
}

void RelayTask(void* params)
{
	while (true)
	{
		//Capture variables (as soon as possible without a mutex).
		ulong _onDuration = onDuration;
		ulong _offDuration = offDuration;
		ulong _lastDurationUpdate = lastDurationUpdate;

		ulong now = millis();

		if (now - _lastDurationUpdate > 3000 || _onDuration == 0 || _offDuration == 0)
		{
			if (now - lastLogTime > 1000)
			{
				std::cout
					<< "Idle"
					<< std::endl;

				lastLogTime = now;
			}

			vTaskDelay(0);
			continue;
		}

		double totalCycleDuration = (_onDuration + _offDuration) / 1000.0;
		double rpm = 60.0 / totalCycleDuration;

		bool applyMultiplier = (rpm >= MIN_RPM && rpm <= MAX_RPM);
		ulong modifiedOnDuration = applyMultiplier ? _onDuration * SPEED_MULTIPLIER : _onDuration;
		ulong modifiedOffDuration = applyMultiplier ? _offDuration * SPEED_MULTIPLIER : _offDuration;

		digitalWrite(RELAY_BASE, HIGH);
		vTaskDelay(pdMS_TO_TICKS(modifiedOnDuration));

		digitalWrite(RELAY_BASE, LOW);
		vTaskDelay(pdMS_TO_TICKS(modifiedOffDuration));

		if (now - lastLogTime > 1000)
		{
			std::cout
				<< "On Duration: " << (_onDuration) << "ms"
				<< ", Off Duration: " << (_offDuration) << "ms"
				<< ", RPM: " << rpm
				<< ", Apply Multiplier: " << applyMultiplier
				<< ", Fake On Duration: " << (modifiedOnDuration) << "ms"
				<< ", Fake Off Duration: " << (modifiedOffDuration) << "ms"
				<< ", Fake RPM: " << (applyMultiplier ? rpm / SPEED_MULTIPLIER : rpm)
				<< std::endl;

			lastLogTime = now;
		}

		vTaskDelay(0);
	}
}

void setup()
{
	Serial.begin(115200);

	pinMode(REED_POSITIVE, OUTPUT);
	pinMode(REED_NEGATIVE, INPUT_PULLDOWN);
	pinMode(RELAY_BASE, OUTPUT);
	pinMode(GPIO_NUM_3, INPUT_PULLDOWN);
	digitalWrite(REED_POSITIVE, HIGH);

	attachInterrupt(REED_NEGATIVE, ReadISR, CHANGE);
	xTaskCreate(RelayTask, "RelayTask", configMINIMAL_STACK_SIZE + 4096, NULL, 5, NULL);
}

void loop()
{
	vTaskDelete(NULL);
}
