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

#define _SET_DATA(index) \
    if (params.contains("d" #index)) \
    { \
        overrides.dataMask[index - 1] = true; \
        overrides.data[index - 1] = std::stoul(params["d" #index], nullptr, 16); \
    }

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
            _SET_DATA(1);
            _SET_DATA(2);
            _SET_DATA(3);
            _SET_DATA(4);
            _SET_DATA(5);
            _SET_DATA(6);
            _SET_DATA(7);
            _SET_DATA(8);

            self->_busMaster->MessageOverrides[id] = overrides;

            _QUERY_OK();
        }

        static esp_err_t OnDataDelete(httpd_req_t* req)
        {
            _QUERY_BOILERPLATE();

            if (!params.contains("id"))
            {
                LOGE(nameof(CAN::API), "Missing 'id' parameter.");
                httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'id' parameter.");
                return ESP_FAIL;
            }
            uint32_t id = std::stoul(params["id"], nullptr, 16);
            self->_busMaster->MessageOverrides.erase(id);

            _QUERY_OK();
        }

        static esp_err_t OnLogPost(httpd_req_t* req)
        {
            _QUERY_BOILERPLATE();

            if (params.empty())
            {
                self->_logger->Whitelist.clear();
                _QUERY_OK();
            }

            //Assume all parameters are valid ids (this is for testing only afterall).
            for (auto& [key, value] : params)
            {
                uint32_t id = std::stoul(key, nullptr, 16);
                self->_logger->Whitelist.push_back(id);
            }

            _QUERY_OK();
        }

        static esp_err_t OnLogDelete(httpd_req_t* req)
        {
            _QUERY_BOILERPLATE();
            for (auto& [key, value] : params)
            {
                uint32_t id = std::stoul(key, nullptr, 16);
                self->_logger->Whitelist.erase(std::remove(self->_logger->Whitelist.begin(), self->_logger->Whitelist.end(), id), self->_logger->Whitelist.end());
            }
            _QUERY_OK();
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
            httpd_uri_t uriDataDelete = {
                .uri = "/can/data",
                .method = HTTP_DELETE,
                .handler = OnDataDelete,
                .user_ctx = this
            };
            if ((err = httpd_register_uri_handler(_server, &uriDataDelete)) != ESP_OK)
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
            httpd_uri_t uriLogDelete = {
                .uri = "/can/log",
                .method = HTTP_DELETE,
                .handler = OnLogDelete,
                .user_ctx = this
            };
            if ((err = httpd_register_uri_handler(_server, &uriLogDelete)) != ESP_OK)
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
