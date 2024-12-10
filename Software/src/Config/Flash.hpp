#pragma once

#include <esp_spiffs.h>
#include <mutex>

namespace ReadieFur::OpenTCU::Config
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
    };
};

std::mutex ReadieFur::OpenTCU::Config::Flash::_mutex;
bool ReadieFur::OpenTCU::Config::Flash::_initialized = false;
