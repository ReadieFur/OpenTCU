#pragma once

#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <mutex>
#include <variant>
#include <WString.h>
#include <type_traits>
#include "pch.h"

namespace ReadieFur::OpenTCU::Config
{
    class JsonFlash
    {
    private:
        typedef std::variant<String, const char*, char*, int, uint, long, ulong, double, float, bool> TValues;

        //Helper trait to check if T is in the variant.
        template <typename T, typename... Types>
        struct is_in_variant : std::disjunction<std::is_same<T, Types>...> {};

        std::mutex _mutex;
        File _writeHandle;
        JsonDocument _jsonDoc;

        JsonFlash(File writeHandle, JsonDocument& jsonDoc) :
        _writeHandle(writeHandle), _jsonDoc(jsonDoc)
        {}

    public:
        static JsonFlash* Open(const char* filename)
        {
            ESP_RETURN_ON_FALSE(SPIFFS.begin(), nullptr, nameof(JsonFlash), "Failed to start SPIFFS.");
            
            String fileContent;
            if (SPIFFS.exists(filename))
            {
                File readHandle = SPIFFS.open(filename, FILE_READ, false);
                ESP_RETURN_ON_FALSE(readHandle, nullptr, nameof(JsonFlash), "Failed to open file '%s' for reading.", filename);

                while (readHandle.available())
                    fileContent += (char)readHandle.read();

                readHandle.close();
            }

            JsonDocument jsonDoc;
            DeserializationError jsonError = deserializeJson(jsonDoc, fileContent);
            ESP_RETURN_ON_FALSE(jsonError.code() == DeserializationError::Code::Ok, nullptr, nameof(JsonFlash), "Failed to parse file '%s': ", filename, jsonError.c_str());

            File writeHandle = SPIFFS.open(filename, FILE_WRITE, false);
            ESP_RETURN_ON_FALSE(writeHandle, nullptr, nameof(JsonFlash), "Failed to open file '%s' for writing.", filename);

            return new JsonFlash(writeHandle, jsonDoc);
        }

        ~JsonFlash()
        {
            _mutex.lock();
            _writeHandle.close();
            _jsonDoc.clear();
            _mutex.unlock();
        }

        template <typename T, typename = std::enable_if_t<is_in_variant<T, TValues>::value>>
        bool TryGet(const char* key, T& outValue)
        {
            _mutex.lock();
            if (!_jsonDoc.containsKey(key))
            {
                _mutex.unlock();
                return false;
            }
            auto val = _jsonDoc[key];
            _mutex.unlock();

            if (!val.is<T>())
                return false;

            outValue = val.as<T>();
            return true;
        }

        template <typename T, typename = std::enable_if_t<is_in_variant<T, TValues>::value>>
        bool Set(const char* key, T value)
        {
            _mutex.lock();

            _jsonDoc[key] = value;
            int bytesWritten = serializeJson(_jsonDoc, _writeHandle);

            _mutex.unlock();

            return bytesWritten > 0;
        }

        bool ContainsKey(const char* key)
        {
            _mutex.lock();
            bool retVal = _jsonDoc.containsKey(key);
            _mutex.unlock();
            return retVal;
        }
    };
};
