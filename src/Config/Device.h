#pragma once

#define ESP_NAME        "ESP_TCU"   //The name this device should appear as under bluetooth settings. TODO: Make this the TCU name with the prefix replaced with ESP.
#define ESP_PIN         ""          //The pin that should be used when pairing with this device.

#define TCU_NAME        ""          //The bluetooth name that the TCU appears as.
#define TCU_PIN         0000        //The pin to connect to the TCU.

#define BAT_CHG         4100        //The voltage (in millivolts) that the device runs at when plugged in.
#define BAT_LOW         3700        //The voltage (in millivolts) for low battery.
#define BAT_CRIT        3600        //The voltage (in millivolts) for critical battery levels.

#define GSM_BAUD        115200      //GSM module baud rate.

#define GPS_BAUD        9600
