#pragma once

#ifdef _DEBUG
#include <freertos/FreeRTOS.h>
#include "Helpers.hpp"
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_task_wdt.h>
#include <freertos/queue.h>
#include "CAN/SCanMessage.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSerialLite.h>
#include <SPIFFS.h>
#include "BusMaster.hpp"
#include <vector>
#include <lwip/sockets.h>

class Debug
{
private:
    static bool _initialized;
    static AsyncWebServer* _debugServer;
    static char* _injectRequestBodyBuffer;
    static struct sockaddr_in _uwpDestAddr;
    static int _uwpSock;

    struct SCanInject
    {
        TickType_t delay;
        uint8_t isSPI;
        SCanMessage message;
    };

    #if defined(ENABLE_SERIAL_CAN_DUMP) || (defined(ENABLE_UWP_SERVER) && defined(ENABLE_UWP_CAN_DUMP))
    static void CanDump(void* arg)
    {
        #if 0
        //Disable the watchdog timer as this method seems to cause the idle task to hang.
        //This isn't ideal but won't be used for production so it'll make do for now.
        //https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/wdts.html
        esp_task_wdt_config_t config = {
            .timeout_ms = 0,
            .idle_core_mask = 0,
            .trigger_panic = false
        };
        esp_task_wdt_reconfigure(&config);
        // esp_task_wdt_deinit();
        #endif

        while (true)
        {
            BusMaster::SCanDump dump;
            if (xQueueReceive(BusMaster::canDumpQueue, &dump, pdMS_TO_TICKS(200)) == pdTRUE)
            {
                #if ENABLE_SERIAL_CAN_DUMP
                printf("[CAN]%d,%d,%x,%d,%d,%d,%x,%x,%x,%x,%x,%x,%x,%x\n",
                    dump.timestamp,
                    dump.isSPI,
                    dump.message.id,
                    dump.message.isExtended,
                    dump.message.isRemote,
                    dump.message.length,
                    dump.message.data[0],
                    dump.message.data[1],
                    dump.message.data[2],
                    dump.message.data[3],
                    dump.message.data[4],
                    dump.message.data[5],
                    dump.message.data[6],
                    dump.message.data[7]
                );
                #endif

                #if ENABLE_UWP_CAN_DUMP
                char txBuffer[128];
                sprintf(txBuffer, "[CAN]%d,%d,%x,%d,%d,%d,%x,%x,%x,%x,%x,%x,%x,%x\n",
                    dump.timestamp,
                    dump.isSPI,
                    dump.message.id,
                    dump.message.isExtended,
                    dump.message.isRemote,
                    dump.message.length,
                    dump.message.data[0],
                    dump.message.data[1],
                    dump.message.data[2],
                    dump.message.data[3],
                    dump.message.data[4],
                    dump.message.data[5],
                    dump.message.data[6],
                    dump.message.data[7]
                );
                int err = sendto(_uwpSock, txBuffer, strlen(txBuffer), 0, (struct sockaddr*)&_uwpDestAddr, sizeof(_uwpDestAddr));
                if (err < 0)
                    ERROR("Failed to send UWP CAN data. %i", err);
                #endif
            }
        }
    }
    #endif

    #ifdef ENABLE_POWER_CHECK
    static void PowerCheck(void* arg)
    {
        #if ENABLE_POWER_CHECK == 0
        // esp_log_level_set(pcTaskGetTaskName(NULL), DEBUG_LOG_LEVEL);

        vTaskDelay(5000 / portTICK_PERIOD_MS); //Wait 5 seconds before starting.
        while (true)
        {
            int level = gpio_get_level(BIKE_POWER_CHECK_PIN);
            DEBUG("Powered: %d", level);
            if (level == 0)
            {
        #endif
                INFO("Powering on");
                gpio_set_level(BIKE_POWER_PIN, 1);
                vTaskDelay(250 / portTICK_PERIOD_MS);
                gpio_set_level(BIKE_POWER_PIN, 0);
        #if ENABLE_POWER_CHECK == 0
            }
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        }
        #endif
    }
    #endif

    #if 0
    static void InjectTask(void* param)
    {
        #pragma region Process data
        std::vector<SCanInject> messages;

        char* linePtr; //Used to save the state between line calls.
        char* tokenPtr; //Used to save the state between token calls (has to remain top-level).
        char* line = strtok_r(_injectRequestBodyBuffer, "\n", &linePtr);
        while (line != nullptr)
        {
            TRACE("Processing line: %s", line);

            //Split the line on commas.
            char* token = strtok_r(line, ",", &tokenPtr);
            bool lineIsInvalid = false;
            SCanInject message;
            int dataLength = -1;
            for (int i = 0; i < 14; i++)
            {
                if (token == nullptr)
                {
                    WARN("Skipping line, not enough tokens.");
                    lineIsInvalid = true;
                    break;
                }

                //Process the token.
                switch (i)
                {
                    case 0:
                        message.delay = atoi(token);
                        break;
                    case 1:
                        message.isSPI = atoi(token);
                        break;
                    case 2:
                        message.message.id = strtol(token, NULL, 16);
                        break;
                    case 3:
                        message.message.isExtended = atoi(token);
                        break;
                    case 4:
                        message.message.isRemote = atoi(token);
                        break;
                    case 5:
                        message.message.length = atoi(token);
                        dataLength = message.message.length;
                        break;
                    case 6:
                        message.message.data[0] = strtol(token, NULL, 16);
                        break;
                    case 7:
                        message.message.data[1] = strtol(token, NULL, 16);
                        break;
                    case 8:
                        message.message.data[2] = strtol(token, NULL, 16);
                        break;
                    case 9:
                        message.message.data[3] = strtol(token, NULL, 16);
                        break;
                    case 10:
                        message.message.data[4] = strtol(token, NULL, 16);
                        break;
                    case 11:
                        message.message.data[5] = strtol(token, NULL, 16);
                        break;
                    case 12:
                        message.message.data[6] = strtol(token, NULL, 16);
                        break;
                    case 13:
                        message.message.data[7] = strtol(token, NULL, 16);
                        break;
                }

                //Don't attempt to process more data than what is specified in the data length.
                if (i >= dataLength + 5)
                    break;

                //Move to the next token.
                token = strtok_r(nullptr, ",", &tokenPtr);
            }

            //Prepare the next line.
            line = strtok_r(nullptr, "\n", &linePtr);

            if (!lineIsInvalid)
                messages.push_back(message);
        }
        #pragma endregion

        #pragma region Send messages
        DEBUG("Injecting %d messages...", messages.size());

        //Raise task priority at this stage.
        vTaskPrioritySet(NULL, RELAY_TASK_PRIORITY);

        #ifdef INJECT_PAUSES_RELAY_TASKS
        //Stop the BusMaster tasks.
        vTaskSuspend(BusMaster::spiToTwaiTaskHandle);
        vTaskSuspend(BusMaster::twaiToSpiTaskHandle);
        #else
        for (int i = 0; i < messages.size(); i++)
            BusMaster::idsToDrop.push_back(messages[i].message.id);
        #endif

        //Send the messages.
        for (int i = 0; i < messages.size(); i++)
        {
            SCanInject message = messages[i];
            if (message.delay > 0)
                vTaskDelay(message.delay);

            TRACE("Injecting message: %d, %d, %x, %d, %d, %d, %x, %x, %x, %x, %x, %x, %x, %x",
                message.delay,
                message.isSPI,
                message.message.id,
                message.message.isExtended,
                message.message.isRemote,
                message.message.length,
                message.message.data[0],
                message.message.data[1],
                message.message.data[2],
                message.message.data[3],
                message.message.data[4],
                message.message.data[5],
                message.message.data[6],
                message.message.data[7]
            );

            esp_err_t sendResult;
            if (message.isSPI)
                sendResult = BusMaster::twaiCan->Send(message.message, CAN_TIMEOUT_TICKS);
            else
                sendResult = BusMaster::spiCan->Send(message.message, CAN_TIMEOUT_TICKS);
            if (sendResult != ESP_OK)
                WARN("Failed to inject message: %x", sendResult);
        }

        #ifdef INJECT_PAUSES_RELAY_TASKS
        //Resume the BusMaster tasks.
        vTaskResume(BusMaster::spiToTwaiTaskHandle);
        vTaskResume(BusMaster::twaiToSpiTaskHandle);
        #else
        BusMaster::idsToDrop.clear();
        #endif

        //Lower task priority at this stage.
        vTaskPrioritySet(NULL, 1);

        DEBUG("Finished injecting messages.");
        #pragma endregion

        delete[] _injectRequestBodyBuffer;
        _injectRequestBodyBuffer = nullptr;
        vTaskDelete(NULL);
    }

    static void HandleInject(AsyncWebServerRequest* request)
    {
        //Triggers when the body has been fully received.
        xTaskCreate(InjectTask, "InjectTask", 4096, NULL, 1, NULL);
        request->send(202);
    }

    static void HandleInjectDataPart(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total)
    {
        //Start of body.
        if (!index)
        {
            if (_injectRequestBodyBuffer)
            {
                WARN("Inject request already in progress.");
                request->send(406);
                return;
            }

            _injectRequestBodyBuffer = new char[total + 1];
            if (!_injectRequestBodyBuffer)
            {
                ERROR("Failed to allocate memory for inject request body.");
                request->send(500);
                return;
            }
        }

        //Body data.
        memcpy(_injectRequestBodyBuffer + index, data, len);

        //End of body.
        if (index + len == total)
            _injectRequestBodyBuffer[total] = '\0';
    }
    #endif

    static void InitTask(void* param)
    {
        // esp_log_level_set("*", DEBUG_LOG_LEVEL);
        INFO("Log level set to %d\n", esp_log_level_get("*"));

        INFO("Debug setup started.");
        WARN("Debug setup started.");
        ERROR("Debug setup started.");
        DEBUG("Debug setup started.");
        TRACE("Debug setup started.");

        // esp_log_level_set("DUMP", DEBUG_LOG_LEVEL);

        #if defined(ENABLE_SERIAL_CAN_DUMP) || (defined(ENABLE_UWP_SERVER) && defined(ENABLE_UWP_CAN_DUMP))
        xTaskCreate(CanDump, "CanDump", 4096, NULL, 1, NULL);
        #endif

        #ifdef ENABLE_POWER_CHECK
        gpio_config_t powerPinConfig = {
            .pin_bit_mask = 1ULL << BIKE_POWER_PIN,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config_t powerCheckPinConfig = {
            .pin_bit_mask = 1ULL << BIKE_POWER_CHECK_PIN,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,
            #if ENABLE_POWER_CHECK == 0
            .intr_type = GPIO_INTR_DISABLE
            #else
            .intr_type = GPIO_INTR_NEGEDGE
            #endif
        };
        assert(gpio_config(&powerPinConfig) == ESP_OK);
        assert(gpio_config(&powerCheckPinConfig) == ESP_OK);
        gpio_install_isr_service(0);
        #if ENABLE_POWER_CHECK == 0
        xTaskCreate(PowerCheck, "PowerCheck", 2048, NULL, 1, NULL);
        #else
        assert(gpio_isr_handler_add(BIKE_POWER_CHECK_PIN, PowerCheck, NULL) == ESP_OK);
        if (gpio_get_level(BIKE_POWER_CHECK_PIN) == 0)
            PowerCheck(NULL);
        #endif
        #endif

        #if defined(ENABLE_WIFI_SERVER) || defined(ENABLE_UWP_SERVER)
        IPAddress ipAddress;
        WiFi.mode(WIFI_AP_STA);
        // IPAddress ip(192, 168, 0, 158);
        // IPAddress subnet(255, 255, 254, 0);
        // IPAddress gateway(192, 168, 1, 254);
        // WiFi.config(ip, gateway, subnet);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        //https://github.com/esphome/issues/issues/4893
        WiFi.setTxPower(WIFI_POWER_8_5dBm);
        if (WiFi.waitForConnectResult() != WL_CONNECTED)
        {
            ERROR("STA Failed!");

            //Create AP using mac address.
            uint8_t mac[6];
            WiFi.macAddress(mac);
            char ssid[18];
            sprintf(ssid, "ESP32-%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            WiFi.softAP(ssid);
            ipAddress = WiFi.softAPIP();
        }
        else
        {
            ipAddress = WiFi.localIP();
        }
        #endif

        #ifdef ENABLE_UWP_SERVER
        //https://gektor650.medium.com/esp-idf-esp32-udp-broadcasts-messages-through-the-wi-fi-4a7f3d75d8ea
        char hostIp[strlen(DEBUG_ADDR)];
        strcpy(hostIp, DEBUG_ADDR);

        //Setup socket.
        _uwpDestAddr.sin_addr.s_addr = inet_addr(hostIp);
        _uwpDestAddr.sin_family = AF_INET;
        _uwpDestAddr.sin_port = htons(3333);
        _uwpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        ASSERT(_uwpSock >= 0);

        //Set timeout (0.5s).
        struct timeval uwpTimeout;
        uwpTimeout.tv_sec = 0;
        uwpTimeout.tv_usec = 500000;
        ASSERT(setsockopt(_uwpSock, SOL_SOCKET, SO_SNDTIMEO, (char*)&uwpTimeout, sizeof(uwpTimeout)) >= 0);
        #endif

        #ifdef ENABLE_WIFI_SERVER
        _debugServer = new AsyncWebServer(80);
        WebSerial.begin(_debugServer);

        #pragma region Inject endpoint
        ASSERT(SPIFFS.begin(true));
        _debugServer->on("/inject", HTTP_GET, [](AsyncWebServerRequest* request) { request->send(SPIFFS, "/inject.html", String(), false); });
        //https://github.com/me-no-dev/ESPAsyncWebServer/issues/123
        _debugServer->on("/inject", HTTP_POST, HandleInject, NULL, HandleInjectDataPart);
        #pragma endregion

        _debugServer->begin();

        DEBUG("Debug server started at %s", ipAddress.toString().c_str());
        #endif

        INFO("Debug setup finished.");
        vTaskDelete(NULL);
    }

public:
    static void Init()
    {
        if (_initialized)
            return;
        _initialized = true;

        esp_log_level_set("*", DEBUG_LOG_LEVEL);
        xTaskCreate(InitTask, "DebugSetup", 4096, NULL, 1, NULL); //Low priority task as it is imperative that the CAN bus is setup first.
    }

    static void Dump(const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        
        char buffer[128];
        vsnprintf(buffer, sizeof(buffer), format, args);
        // ESP_LOG_LEVEL_LOCAL(ESP_LOG_VERBOSE, "DUMP", "%s", buffer);
        puts(buffer);
        
        #ifdef ENABLE_WIFI_SERVER
        WebSerial.printf("[DUMP] %s\n", buffer);
        #endif

        va_end(args);
    }
};

bool Debug::_initialized = false;
AsyncWebServer* Debug::_debugServer = nullptr;
char* Debug::_injectRequestBodyBuffer = nullptr;
struct sockaddr_in Debug::_uwpDestAddr = {};
int Debug::_uwpSock = -1;
#endif
