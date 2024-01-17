#pragma once

#pragma region ESP-NOW Implementation
//https://randomnerdtutorials.com/esp-now-two-way-communication-esp32/

#include <esp_now.h>
#include <WiFi.h>

class TwoChip
{
private:
    static esp_now_peer_info_t peerInfo;

    // #ifdef DEBUG
    // static void OnSend(const uint8_t* mac, esp_now_send_status_t status)
    // {
    //     printf("Last Packet Send Status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
    // };
    // #endif

    static void OnReceive(const uint8_t* mac, const uint8_t* data, int dataLength)
    {
        // #ifdef DEBUG
        // printf("Received from %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        // #endif

        if (receiveCallback == nullptr)
            return;

        receiveCallback(data, dataLength);
    };
public:
    static void (*receiveCallback)(const uint8_t* data, uint8_t dataSize);

    static esp_err_t Begin(const uint8_t* peerMac)
    {
        if (peerMac == nullptr)
            return ESP_ERR_INVALID_STATE;

        //Check WiFi mode, enforce the mode that has been set by the user but ensure that STA mode is enabled.
        switch (WiFi.getMode())
        {
        case WIFI_MODE_NULL:
            WiFi.mode(WIFI_MODE_STA);
            break;
        case WIFI_MODE_AP:
            WiFi.mode(WIFI_MODE_APSTA);
            break;
        case WIFI_MODE_STA:
        case WIFI_MODE_APSTA:
        default:
            break;
        }

        //Initialize ESP-NOW.
        if (esp_err_t err = esp_now_init() != ESP_OK)
            return err;

        //Register peer.
        memcpy(peerInfo.peer_addr, peerMac, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;

        if (esp_err_t err = esp_now_add_peer(&peerInfo) != ESP_OK)
            return err;

        //Register callbacks.
        // #ifdef DEBUG
        // if (esp_err_t err = esp_now_register_send_cb(OnSend) != ESP_OK)
        //     return err;
        // #endif

        if (esp_err_t err = esp_now_register_recv_cb(OnReceive) != ESP_OK)
            return err;

        return ESP_OK;
    };

    static esp_err_t End()
    {
        return esp_now_del_peer(peerInfo.peer_addr);
    };

    static esp_err_t Send(uint8_t* data, uint8_t dataLength)
    {
        return esp_now_send(peerInfo.peer_addr, data, dataLength);
    };
};

//Static initializers.
esp_now_peer_info_t TwoChip::peerInfo = {};
void (*TwoChip::receiveCallback)(const uint8_t* data, uint8_t dataSize) = nullptr;
#pragma endregion

#pragma region I2C Implementation
// #include <driver/gpio.h>
// #include <Arduino.h>
// #include <Wire.h>

// class TwoChip
// {
// private:
//     static bool _initialized;
//     static bool _isMaster;
//     static uint32_t _frequency;
//     static TaskHandle_t taskHandle;
//     static void (*_onReceiveCallback)(uint8_t* data, uint8_t dataSize);

//     static void HandleReceive(int bytes)
//     {
//         if (_onReceiveCallback == nullptr)
//             return;

//         if (_isMaster)
//         {
//             //TODO
//         }
//         else
//         {
//             uint8_t data[bytes];
//             Wire1.readBytes(data, bytes);
//             _onReceiveCallback(data, bytes);
//         }
//     }

//     static void HandleSlaveRequest()
//     {
//         //TODO
//     }

//     static void Task(void* arg)
//     {
//         while (true)
//         {
//             if (_isMaster)
//             {
//                 //Poll every slave possible for data.
//                 for (uint8_t address = 1; address < 128; address++)
//                 {
//                     Wire1.beginTransmission(address);
//                     Wire1.write(0);
//                     Wire1.endTransmission();

//                     //If there is data, read it.
//                     if (Wire1.requestFrom(address, 1) == 1)
//                     {
//                         uint8_t data[1];
//                         Wire1.readBytes(data, 1);
//                         _onReceiveCallback(data, 1);
//                     }
//                 }
//             }
//             else
//             {
//                 //TODO
//             }

//             //Use frequency to determine delay.
//             vTaskDelay(1000 / _frequency / portTICK_PERIOD_MS);
//         }
//     }

// public:
//     static void Begin(gpio_num_t rxPin, gpio_num_t txPin, uint32_t frequency, uint8_t address)
//     {
//         if (_initialized)
//             return;
//         _initialized = true;

//         _isMaster = address == 0;
//         _frequency = frequency;

//         //Use Wire1 to avoid conflicts with Wire.
//         if (_isMaster)
//         {
//             Wire1.begin(rxPin, txPin, frequency);
//             Wire1.onReceive(HandleReceive);
//         }
//         else
//         {
//             Wire1.begin(address, rxPin, txPin, frequency);
//             Wire1.onReceive(HandleReceive);
//             Wire1.onRequest(HandleSlaveRequest);
//         }

//         xTaskCreate(Task, "TwoChip", 1024, NULL, 1, &taskHandle);
//     };

//     static void End()
//     {
//         if (!_initialized)
//             return;
//         _initialized = false;
//         vTaskDelete(taskHandle);
//         Wire1.end();
//     };
// };
#pragma endregion
