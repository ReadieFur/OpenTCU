#include "Logging.h" //Include this first so that it takes priority over the ESP-IDF logging macros.
#include <freertos/FreeRTOS.h> //Has to always be the first included FreeRTOS related header.
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/twai.h>

#define CAN_TIMEOUT_TICKS pdMS_TO_TICKS(50)

void setup()
{
    vTaskDelay(pdMS_TO_TICKS(1000));

    // esp_log_level_set("*", DEBUG_LOG_LEVEL);
    LOG_INFO("Log level set to: %d", esp_log_level_get("*"));

    LOG_INFO("Debug setup started.");
    LOG_WARN("Debug setup started.");
    LOG_ERROR("Debug setup started.");
    LOG_DEBUG("Debug setup started.");
    LOG_TRACE("Debug setup started.");

    #if defined(STA_WIFI_SSID) && defined(STA_WIFI_PASSWORD)
    WiFi.mode(WIFI_AP_STA);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    WiFi.begin(STA_WIFI_SSID, STA_WIFI_PASSWORD);

    for (size_t i = 0; i < 10; i++)
    {
        if (WiFi.waitForConnectResult() == WL_CONNECTED)
            break;

        if (i == 9)
        {
            LOG_ERROR("Failed to connect to STA network.");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (WiFi.isConnected())
    {
        LOG_INFO("STA IP Address: %s", WiFi.localIP().toString());
            
        #ifdef NTP_SERVER
        //Time setup is done here to get the actual time, if this stage is skipped the internal ESP tick count is used for the logs.
        //Setup RTC for logging (mainly to sync external data to data recorded from this device, e.g. pairing video with recorded data).
        LOG_DEBUG("Syncing time with NTP server...");
        for (int i = 0; i < 10; i++)
        {
            struct tm timeInfo;
            if (getLocalTime(&timeInfo))
            {
                long timestamp;
                time(&timestamp);
                LOG_DEBUG("Successfully synced with timeserver. %i/%i/%i %i:%i:%i %li %li",
                    timeInfo.tm_year,
                    timeInfo.tm_mon,
                    timeInfo.tm_yday,
                    timeInfo.tm_hour,
                    timeInfo.tm_min,
                    timeInfo.tm_sec,
                    timestamp,
                    xTaskGetTickCount()
                );
                break;
            }
            else if (i == 9)
            {
                LOG_ERROR("Failed to sync with timeserver.");
                break;
            }

            //https://randomnerdtutorials.com/esp32-date-time-ntp-client-server-arduino/
            configTime(0, 3600, NTP_SERVER);
        }
        #endif
    }
    #endif

    //Configure GPIO.
    gpio_config_t txPinConfig = {
        .pin_bit_mask = 1ULL << TWAI_TX_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config_t rxPinConfig = {
        .pin_bit_mask = 1ULL << TWAI_RX_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ASSERT(gpio_config(&txPinConfig) == ESP_OK);
    ASSERT(gpio_config(&rxPinConfig) == ESP_OK);

    twai_general_config_t generalConfig = TWAI_GENERAL_CONFIG_DEFAULT(
        TWAI_TX_PIN,
        TWAI_RX_PIN,
        TWAI_MODE_NORMAL
    );
    twai_timing_config_t timingConfig = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t filterConfig = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ASSERT(twai_driver_install(&generalConfig, &timingConfig, &filterConfig) == ESP_OK);
    ASSERT(twai_start() == ESP_OK);
    ASSERT(twai_reconfigure_alerts(TWAI_ALERT_RX_DATA, NULL) == ESP_OK);

    LOG_INFO("Setup completed.");
}

#ifdef ARDUINO
void loop()
{
    //Use the read alerts function to wait for a message to be received (instead of locking on the twai_receive function).
    LOG_TRACE("TWAI Wait");

    uint32_t alerts;
    if (esp_err_t err = twai_read_alerts(&alerts, CAN_TIMEOUT_TICKS) != ESP_OK)
    {
        LOG_TRACE("TWAI Wait Timeout: %d", err);
        return;
    }
    //We don't need to check the alert type because we have only subscribed to the RX_DATA alert.
    // else if (!(alerts & TWAI_ALERT_RX_DATA))
    // {
    //     // TRACE("TWAI Wait Timeout: %d", alerts);
    //     return ESP_ERR_INVALID_RESPONSE;
    // }

    LOG_TRACE("TWAI Read");

    twai_message_t twaiMessage;
    esp_err_t err = twai_receive(&twaiMessage, CAN_TIMEOUT_TICKS);

    ulong millisTimestamp = xTaskGetTickCount();
    time_t unixTimestamp = 0;
    struct tm timeInfo;
    if (getLocalTime(&timeInfo))
        time(&unixTimestamp);

    if (err != ESP_OK)
    {
        LOG_ERROR("TWAI Read Error: %d", err);
        return;
    }

    LOG_TRACE("TWAI Read Done: %x, %d, %d, %d, %x", twaiMessage.identifier, twaiMessage.data_length_code, twaiMessage.extd, twaiMessage.rtr, twaiMessage.data[0]);

    struct SCanMessage
    {
        uint32_t id;
        uint8_t data[8];
        uint8_t length;
        bool isExtended;
        bool isRemote;
    } message =
    {
        .id = twaiMessage.identifier,
        .length = twaiMessage.data_length_code,
        .isExtended = twaiMessage.extd,
        .isRemote = twaiMessage.rtr
    };
    for (int i = 0; i < twaiMessage.data_length_code; i++)
        message.data[i] = twaiMessage.data[i];

    struct SCanDump
    {
        long timestamp;
        long timestamp2;
        bool isSpi;
        SCanMessage message;
    } currentDump =
    {
        .timestamp = unixTimestamp,
        .timestamp2 = millisTimestamp,
        .isSpi = false,
        .message = message
    };

    printf("[CAN]%d %d,%d,%x,%d,%d,%d,%x,%x,%x,%x,%x,%x,%x,%x\n",
        currentDump.timestamp,
        currentDump.timestamp2,
        currentDump.isSpi,
        currentDump.message.id,
        currentDump.message.isExtended,
        currentDump.message.isRemote,
        currentDump.message.length,
        currentDump.message.data[0],
        currentDump.message.data[1],
        currentDump.message.data[2],
        currentDump.message.data[3],
        currentDump.message.data[4],
        currentDump.message.data[5],
        currentDump.message.data[6],
        currentDump.message.data[7]);
}
#else
extern "C" void app_main()
{
    setup();
    //app_main IS allowed to return as per the ESP32 documentation (other FreeRTOS tasks will continue to run).
}
#endif
