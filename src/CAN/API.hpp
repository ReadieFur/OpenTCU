#pragma once

#include <Service/AService.hpp>
#include <esp_http_server.h>
#include <esp_ota_ops.h>
#include <esp_log.h>
#include <esp_system.h>
#include <Logging.hpp>
#include <freertos/semphr.h>
#include <Network/WiFi.hpp>
#include "BusMaster.hpp"
#include "Logger.hpp"
#include <map>
#include <string>
#include <sstream>

#define _QUERY_BOILERPLATE() \
    API* self = static_cast<API*>(req->user_ctx); \
    char buf[100]; \
    esp_err_t err = ESP_OK; \
    if ((err = ReadQuery(buf, sizeof(buf), req)) != ESP_OK) \
    { \
        LOGE(nameof(CAN::API), "Failed to read query: %s", esp_err_to_name(err)); \
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid query."); \
        return err; \
    } \
    std::map<std::string, std::string> params = GetQueryParameters(buf);

#define _QUERY_OK() \
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN); \
    return ESP_OK;

namespace ReadieFur::OpenTCU::CAN
{
    class API : public Service::AService
    {
    private:
        BusMaster* _busMaster = nullptr;
        Logger* _logger = nullptr;
        httpd_config_t _config = HTTPD_DEFAULT_CONFIG();
        httpd_handle_t _server = NULL;

        static std::map<std::string, std::string> GetQueryParameters(const char* queryString)
        {
            std::map<std::string, std::string> queryParams;

            if (!queryString)
                return queryParams;

            std::istringstream queryStream(queryString);
            std::string pair;
            while (std::getline(queryStream, pair, '&'))
            {
                size_t delimiter_pos = pair.find('=');
                if (delimiter_pos != std::string::npos)
                {
                    std::string key = pair.substr(0, delimiter_pos);
                    std::string value = pair.substr(delimiter_pos + 1);
                    queryParams[key] = value;
                }
                else
                {
                    queryParams[pair] = "";
                }
            }
            return queryParams;
        }

        static esp_err_t ReadQuery(char* buf, size_t bufSize, httpd_req_t* req)
        {
            size_t len = httpd_req_get_url_query_len(req) + 1;

            if (len <= 1)
            {
                buf[0] = '\0';
                return ESP_OK;
            }

            if (len > bufSize)
                return ESP_FAIL;

            esp_err_t err = httpd_req_get_url_query_str(req, buf, len);
            if (err != ESP_OK)
                return err;

            return ESP_OK;
        }

        static void SetData(int index, std::map<std::string, std::string>& params, BusMaster::SMessageOverrides& overrides)
        {
            if (params.contains("d" + std::to_string(index)))
            {
                overrides.dataMask[index - 1] = true;
                overrides.data[index - 1] = std::stoul(params["d" + std::to_string(index)], nullptr, 16);
            }
        }

        static esp_err_t OnDataPost(httpd_req_t* req)
        {
            _QUERY_BOILERPLATE();

            if (!params.contains("id"))
            {
                LOGE(nameof(CAN::API), "Missing 'id' parameter.");
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'id' parameter.");
                return ESP_FAIL;
            }
            uint32_t id = std::stoul(params["id"], nullptr, 16);

            BusMaster::SMessageOverrides overrides;
            SetData(1, params, overrides);
            SetData(2, params, overrides);
            SetData(3, params, overrides);
            SetData(4, params, overrides);
            SetData(5, params, overrides);
            SetData(6, params, overrides);
            SetData(7, params, overrides);
            SetData(8, params, overrides);

            self->_busMaster->MessageOverrides[id] = overrides;

            _QUERY_OK();
        }

        static void SetReplacement(int index, std::map<std::string, std::string>& params, BusMaster::SMessageReplacements& replacements)
        {
            if (params.contains("o" + std::to_string(index)) && params.contains("r" + std::to_string(index)))
            {
                replacements.dataMask[index - 1] = true;
                replacements.original[index - 1] = std::stoul(params["o" + std::to_string(index)], nullptr, 16);
                replacements.replacement[index - 1] = std::stoul(params["r" + std::to_string(index)], nullptr, 16);
            }
        }

        static esp_err_t OnReplacePost(httpd_req_t* req)
        {
            _QUERY_BOILERPLATE();

            if (!params.contains("id"))
            {
                LOGE(nameof(CAN::API), "Missing 'id' parameter.");
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'id' parameter.");
                return ESP_FAIL;
            }
            uint32_t id = std::stoul(params["id"], nullptr, 16);
            
            BusMaster::SMessageReplacements replacements;
            replacements.id = id;
            SetReplacement(1, params, replacements);
            SetReplacement(2, params, replacements);
            SetReplacement(3, params, replacements);
            SetReplacement(4, params, replacements);
            SetReplacement(5, params, replacements);
            SetReplacement(6, params, replacements);
            SetReplacement(7, params, replacements);
            SetReplacement(8, params, replacements);

            //Check if a replacement already exists with a matching ID, mask and original data.
            for (auto& replacement : self->_busMaster->MessageReplacements)
            {
                if (replacement.id == replacements.id)
                {
                    bool match = true;
                    for (int i = 0; i < 8; i++)
                    {
                        if (replacement.dataMask[i] != replacements.dataMask[i]
                            || replacement.original[i] != replacements.original[i])
                        {
                            match = false;
                            break;
                        }
                    }
                    if (match)
                    {
                        replacement = replacements;
                        _QUERY_OK();
                    }
                }
            }

            self->_busMaster->MessageReplacements.push_back(replacements);

            _QUERY_OK();
        }

        static esp_err_t OnDropPost(httpd_req_t* req)
        {
            _QUERY_BOILERPLATE();

            self->_busMaster->Blacklist.clear();
            for (auto& [key, value] : params)
            {
                uint32_t id = std::stoul(key, nullptr, 16);
                self->_busMaster->Blacklist.push_back(id);
            }

            _QUERY_OK();
        }

        static esp_err_t OnLogPost(httpd_req_t* req)
        {
            _QUERY_BOILERPLATE();

            //Always provide all IDs to whitelist.
            self->_logger->Whitelist.clear();

            //Assume all parameters are valid ids (this is for testing only afterall).
            for (auto& [key, value] : params)
            {
                uint32_t id = std::stoul(key, nullptr, 16);
                self->_logger->Whitelist.push_back(id);
            }

            _QUERY_OK();
        }

        static esp_err_t OnInjectPost(httpd_req_t* req)
        {
            _QUERY_BOILERPLATE();

            SCanMessage message;

            if (!params.contains("id"))
            {
                LOGE(nameof(CAN::API), "Missing 'id' parameter.");
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'id' parameter.");
                return ESP_FAIL;
            }
            message.id = std::stoul(params["id"], nullptr, 16);

            if (!params.contains("bus"))
            {
                LOGE(nameof(CAN::API), "Missing 'bus' parameter.");
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'bus' parameter.");
                return ESP_FAIL;
            }
            uint32_t bus = std::stoul(params["bus"]);

            if (!params.contains("length"))
            {
                LOGE(nameof(CAN::API), "Missing 'length' parameter.");
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'length' parameter.");
                return ESP_FAIL;
            }
            message.length = std::stoul(params["length"]);

            for (int i = 0; i < message.length; i++)
            {
                if (!params.contains("d" + std::to_string(i + 1)))
                {
                    LOGE(nameof(CAN::API), "Missing 'd%i' parameter.", i + 1);
                    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing a 'd' parameter.");
                    return ESP_FAIL;
                }
                message.data[i] = std::stoul(params["d" + std::to_string(i + 1)], nullptr, 16);
            }

            if (bus == 1)
                self->_busMaster->InjectQueue0.push(message);
            else if (bus == 2)
                self->_busMaster->InjectQueue1.push(message);
            else
            {
                LOGE(nameof(CAN::API), "Invalid 'bus' parameter.");
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid 'bus' parameter.");
                return ESP_FAIL;
            }

            _QUERY_OK();
        }

        static esp_err_t OnRebootPost(httpd_req_t* req)
        {
            httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
            vTaskDelay(pdMS_TO_TICKS(100));
            esp_restart();
            return ESP_OK;
        }

    protected:
        void RunServiceImpl() override
        {
            _busMaster = GetService<BusMaster>();
            _logger = GetService<Logger>();

            if (!Network::WiFi::Initalized())
                return;

            esp_err_t err = ESP_OK;
            _config.server_port = 8080;
            if ((err = httpd_start(&_server, &_config)) != ESP_OK)
            {
                LOGE(nameof(CAN::API), "Failed to start HTTP server.");
                return;
            }

            httpd_uri_t uriDataPost = {
                .uri = "/can/data",
                .method = HTTP_POST,
                .handler = OnDataPost,
                .user_ctx = this
            };
            if ((err = httpd_register_uri_handler(_server, &uriDataPost)) != ESP_OK)
            {
                LOGE(nameof(CAN::API), "Failed to register URI handler.");
                return;
            }

            httpd_uri_t uriDropPost = {
                .uri = "/can/drop",
                .method = HTTP_POST,
                .handler = OnDropPost,
                .user_ctx = this
            };
            if ((err = httpd_register_uri_handler(_server, &uriDropPost)) != ESP_OK)
            {
                LOGE(nameof(CAN::API), "Failed to register URI handler.");
                return;
            }

            httpd_uri_t uriReplacePost = {
                .uri = "/can/replace",
                .method = HTTP_POST,
                .handler = OnReplacePost,
                .user_ctx = this
            };
            if ((err = httpd_register_uri_handler(_server, &uriReplacePost)) != ESP_OK)
            {
                LOGE(nameof(CAN::API), "Failed to register URI handler.");
                return;
            }

            httpd_uri_t uriLogPost = {
                .uri = "/can/log",
                .method = HTTP_POST,
                .handler = OnLogPost,
                .user_ctx = this
            };
            if ((err = httpd_register_uri_handler(_server, &uriLogPost)) != ESP_OK)
            {
                LOGE(nameof(CAN::API), "Failed to register URI handler.");
                return;
            }

            httpd_uri_t uriInjectPost = {
                .uri = "/can/inject",
                .method = HTTP_POST,
                .handler = OnInjectPost,
                .user_ctx = this
            };
            if ((err = httpd_register_uri_handler(_server, &uriInjectPost)) != ESP_OK)
            {
                LOGE(nameof(CAN::API), "Failed to register URI handler.");
                return;
            }

            httpd_uri_t uriRebootPost = {
                .uri = "/can/reboot",
                .method = HTTP_POST,
                .handler = OnRebootPost,
                .user_ctx = this
            };
            if ((err = httpd_register_uri_handler(_server, &uriRebootPost)) != ESP_OK)
            {
                LOGE(nameof(CAN::API), "Failed to register URI handler.");
                return;
            }

            ServiceCancellationToken.WaitForCancellation();

            httpd_stop(_server);
            _server = NULL;
            _busMaster = nullptr;
        }

    public:
        API()
        {
            ServiceEntrypointStackDepth += 1024;
            AddDependencyType<BusMaster>();
            AddDependencyType<Logger>();
        }
    };
};
