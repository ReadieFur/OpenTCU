#pragma once

#include "WiFi.h"

class PowerManager
{
private:
public:
    static void Begin()
    {
        WiFi.mode(WIFI_OFF);
        WiFi.setSleep(true);
    }
};
