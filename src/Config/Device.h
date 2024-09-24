#pragma once

#define TCU_NAME        "WSBC001130512T"//The bluetooth name that the TCU appears as.
#define TCU_PIN         "0000"      //The pin to connect to the TCU.

#define BAT_CHG         4100        //The voltage (in millivolts) that the device runs at when plugged in.
#define BAT_LOW         3700        //The voltage (in millivolts) for low battery.
#define BAT_CRIT        3600        //The voltage (in millivolts) for critical battery levels.
#define BAT_CRIT_INT    300000      //The time (in milliseconds) for the module to sleep before waking up again.

#define GSM_BAUD        115200      //GSM module baud rate.

#define GPS_BAUD        9600
