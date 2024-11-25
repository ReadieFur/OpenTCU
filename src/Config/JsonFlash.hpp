#pragma once

#include <esp_spiffs.h>
#include <ArduinoJson.h>
#include <mutex>
#include <variant>
#include <string>
#include <type_traits>
#include "pch.h"

namespace ReadieFur::OpenTCU::Config
{
    class JsonFlash
    {
    private:
        using TValues = ::std::variant<std::string, const char*, char*, int, uint, long, ulong, double, float, bool>;

        //Helper trait to check if T is in the variant.
        template <typename T, typename Variant>
        struct is_variant_type;
        template <typename T, typename... Types>
        struct is_variant_type<T, std::variant<Types...>> : std::disjunction<std::is_same<T, Types>...> {};

        std::mutex _mutex;
        FILE* _writeHandle;
        JsonDocument _jsonDoc;

        JsonFlash(FILE* writeHandle, JsonDocument& jsonDoc) :
        _writeHandle(writeHandle), _jsonDoc(jsonDoc)
        {}

    public:
        static JsonFlash* Open(const char* filename)
        {
            // ESP_RETURN_ON_FALSE(SPIFFS.begin(), nullptr, nameof(JsonFlash), "Failed to start SPIFFS.");
            esp_vfs_spiffs_conf_t conf = {
                .base_path = "/",
                .partition_label = NULL,
                .max_files = 5,
                .format_if_mount_failed = true
            };
            ESP_RETURN_ON_FALSE(esp_vfs_spiffs_register(&conf) == ESP_OK, nullptr, nameof(JsonFlash), "Failed to register SPIFFS.");

            std::string fileContent;
            if (access(filename, F_OK) == 0)
            {
                FILE* readHandle = fopen(filename, "r");
                ESP_RETURN_ON_FALSE(readHandle == nullptr, nullptr, nameof(JsonFlash), "Failed to open file '%s' for reading.", filename);

                char buffer[256];
                while (fgets(buffer, sizeof(buffer), readHandle) != nullptr)
                    fileContent.append(buffer);
                fclose(readHandle);
            }

            JsonDocument jsonDoc;
            DeserializationError jsonError = deserializeJson(jsonDoc, fileContent);
            ESP_RETURN_ON_FALSE(jsonError.code() == DeserializationError::Code::Ok, nullptr, nameof(JsonFlash), "Failed to parse file '%s': ", filename, jsonError.c_str());

            FILE* writeHandle = fopen(filename, "w");
            ESP_RETURN_ON_FALSE(writeHandle == nullptr, nullptr, nameof(JsonFlash), "Failed to open file '%s' for writing.", filename);

            return new JsonFlash(writeHandle, jsonDoc);
        }

        ~JsonFlash()
        {
            _mutex.lock();
            if (_writeHandle != nullptr)
                fclose(_writeHandle);
            _jsonDoc.clear();
            _mutex.unlock();
        }

        template <typename T>
        typename std::enable_if<is_variant_type<T, TValues>::value, bool>::type
        TryGet(const char* key, T& outValue)
        {
            _mutex.lock();
            auto val = _jsonDoc[key];
            _mutex.unlock();

            if (!val.is<T>())
                return false;

            outValue = val.as<T>();
            return true;
        }

        template <typename T>
        typename std::enable_if<is_variant_type<T, TValues>::value, bool>::type
        Set(const char* key, T value)
        {
            _mutex.lock();

            _jsonDoc[key] = value;

            fseek(_writeHandle, 0, SEEK_SET);
            size_t bytesWritten = serializeJson(_jsonDoc, *_writeHandle);
            fflush(_writeHandle);

            _mutex.unlock();

            return bytesWritten > 0;
        }

        template <typename T>
        typename std::enable_if<is_variant_type<T, TValues>::value, bool>::type
        ContainsKey(const char* key)
        {
            _mutex.lock();
            bool retVal = _jsonDoc[key].is<T>();
            _mutex.unlock();
            return retVal;
        }
    };
};
