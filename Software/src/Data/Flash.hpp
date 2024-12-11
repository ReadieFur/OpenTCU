#pragma once

#include <esp_spiffs.h>
#include <mutex>
#include <ArduinoJson.h>

#define JSON_ASSIGN_TO_SOURCE_IF_TYPE(doc, src, type) if (doc[#src].is<type>()) src = doc[#src].as<type>();
#define JSON_SET_PROP(doc, src) doc[#src] = src;

namespace ReadieFur::OpenTCU::Data
{
    class Flash
    {
    private:
        static std::mutex _mutex;
        static bool _initialized;

    public:
        static esp_err_t Init()
        {
            _mutex.lock();

            if (esp_spiffs_mounted(NULL))
            {
                _mutex.unlock();
                return ESP_ERR_INVALID_STATE;
            }

            esp_vfs_spiffs_conf_t conf = {
                .base_path = "/spiffs",
                .partition_label = NULL,
                .max_files = 5,
                .format_if_mount_failed = true
            };

            esp_err_t err;
            if ((err = esp_vfs_spiffs_register(&conf)) != ESP_OK)
            {
                _mutex.unlock();
                return err;
            }

            _initialized = true;

            _mutex.unlock();
            return ESP_OK;
        }

        static esp_err_t Deinit()
        {
            _mutex.lock();

            if (!esp_spiffs_mounted(NULL))
            {
                _mutex.unlock();
                return ESP_OK;
            }

            esp_err_t err = esp_vfs_spiffs_unregister(NULL);
            if (err == ESP_OK)
                _initialized = false;

            _mutex.unlock();
            return err;
        }

        static bool IsInitialized()
        {
            return _initialized;
        }

        static esp_err_t Read(const char* path, char* outBuffer, size_t* outLength)
        {
            _mutex.lock();

            if (!_initialized)
            {
                _mutex.unlock();
                return ESP_ERR_INVALID_STATE;
            }

            if (outLength == nullptr)
            {
                _mutex.unlock();
                return ESP_ERR_INVALID_ARG;
            }

            FILE* file = fopen(path, "r");
            if (file == NULL)
            {
                _mutex.unlock();
                return ESP_ERR_NOT_FOUND;
            }

            //If the buffer is null, return the length to the caller.
            if (outBuffer == nullptr)
            {
                //Scan over the file to get the size.
                fseek(file, 0, SEEK_END);
                *outLength = ftell(file);
                fclose(file);
                return ESP_OK;
            }

            size_t bytesRead = fread(outBuffer, 1, *outLength, file);
            fclose(file);

            _mutex.unlock();
            return bytesRead > 0 ? ESP_OK : ESP_FAIL;
        }

        static esp_err_t Write(const char* path, const char* buffer, size_t length)
        {
            _mutex.lock();

            if (!_initialized)
            {
                _mutex.unlock();
                return ESP_ERR_INVALID_STATE;
            }

            FILE* file = fopen(path, "w");
            if (file == NULL)
            {
                _mutex.unlock();
                return ESP_ERR_NOT_FOUND;
            }

            size_t bytesWritten = fwrite(buffer, 1, length, file);
            fclose(file);

            _mutex.unlock();
            return bytesWritten > 0 ? ESP_OK : ESP_FAIL;
        }

        static esp_err_t LoadJson(const char* path, JsonDocument& document)
        {
            if (!_initialized)
                return ESP_ERR_INVALID_STATE;

            //Load config after starting core tasks as the core tasks can function without the config.
            size_t configSize;
            esp_err_t err;
            if ((err = Data::Flash::Read(path, nullptr, &configSize)) != ESP_OK)
                return err;
            
            char configBuffer[configSize];
            if ((err = Data::Flash::Read(path, configBuffer, &configSize)) != ESP_OK)
                return err;
            
            DeserializationError jsonErr = deserializeJson(document, configBuffer);
            return jsonErr == DeserializationError::Ok ? ESP_OK : ESP_FAIL;
        }

        static esp_err_t SaveJson(const char* path, JsonDocument& document)
        {
            if (!_initialized)
                return ESP_ERR_INVALID_STATE;

            char buffer[measureJson(document)];
            serializeJson(document, buffer, sizeof(buffer));
            return Data::Flash::Write(path, buffer, sizeof(buffer));
        };
    };
};

std::mutex ReadieFur::OpenTCU::Data::Flash::_mutex;
bool ReadieFur::OpenTCU::Data::Flash::_initialized = false;
