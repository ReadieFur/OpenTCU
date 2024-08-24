#pragma once

#include "Logging.h"
#include "WiFi.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>

class Webservices
{
private:
    static AsyncWebServer* _webserver;
    static ulong _lastOtaLog;

    #pragma region OTA
    static void OnOtaStart()
    {
        LOG_TRACE("OTA update started.");
    }

    static void OnOtaProgress(size_t current, size_t final)
    {
        //Log at most every 1 seconds.
        if (millis() - _lastOtaLog > 1000)
        {
            _lastOtaLog = millis();
            LOG_INFO("OTA Progress: %u/%u bytes\n", current, final);
        }
    }

    static void OnOtaEnd(bool success)
    {
        // Log when OTA has finished
        if (success)
        {
            LOG_INFO("Rebooting to install OTA update...");
            // vTaskDelay(pdMS_TO_TICKS(2000));
            ESP.restart();
        }
        else
            LOG_ERROR("Error during OTA update!");
    }
    #pragma endregion

public:
    static void Begin()
    {
        #pragma region WiFi
        switch (WiFi.getMode())
        {
        case WIFI_STA:
            WiFi.mode(WIFI_AP_STA);
            break;
        default:
            WiFi.mode(WIFI_AP);
            break;
        }

        uint8_t macAddress[6];
        esp_efuse_mac_get_default(macAddress);
        char apSsid[18];
        sprintf(apSsid, "ESP32-%02X%02X%02X%02X%02X%02X", apSsid[0], apSsid[1], apSsid[2], apSsid[3], apSsid[4], apSsid[5]);
        WiFi.softAP(apSsid, AP_WIFI_PASSWORD);

        //https://github.com/esphome/issues/issues/4893
        WiFi.setTxPower(WIFI_POWER_8_5dBm);
        #pragma endregion

        _webserver = new AsyncWebServer(80);

        ElegantOTA.onStart(OnOtaStart);
        ElegantOTA.onProgress(OnOtaProgress);
        ElegantOTA.onEnd(OnOtaEnd);
        ElegantOTA.begin(_webserver);

        _webserver->begin();
    }
};

AsyncWebServer* Webservices::_webserver = nullptr;
ulong Webservices::_lastOtaLog = 0;
