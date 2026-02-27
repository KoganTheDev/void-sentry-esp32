#pragma once

// TODO: Change name to the header file and split to cpp file as well
#include "camera.h"
#include <esp_http_server.h>
#include <esp_log.h>

static const char* TAG_WEBSOCKETS = "WEBSOCKETS";

class WebsocketHandler
{
public:
    WebsocketHandler(httpd_handle_t server_handle, const char* uri)
        : _server_handle(server_handle), _uri(uri), _is_registered(false)
    {
    }

    virtual ~WebsocketHandler() { this->unregister_endpoint(); }

    void register_endpoint()
    {
        ESP_LOGI(TAG_WEBSOCKETS, "WebSocket: Register endpoint %s", this->_uri);
        httpd_uri_t uri_obj = {0};
        uri_obj.uri = this->_uri;
        uri_obj.method = HTTP_GET;
        uri_obj.user_ctx = this;
        uri_obj.handler = WebsocketHandler::websocket_handler;
        uri_obj.is_websocket = true;

        httpd_register_uri_handler(this->_server_handle, &uri_obj);
    }

    void unregister_endpoint()
    {
        ESP_LOGI(TAG_WEBSOCKETS, "WebSocket: Unregister endpoint %s", this->_uri);
        httpd_unregister_uri_handler(this->_server_handle, this->_uri, HTTP_GET);
    }

    static esp_err_t websocket_handler(httpd_req_t* req)
    {
        WebsocketHandler* _this = reinterpret_cast<WebsocketHandler*>(req->user_ctx);

        if (req->method == HTTP_GET)
        {
            // Initial WebSocket upgrade handshake
            ESP_LOGI(TAG_WEBSOCKETS, "WebSocket: New connection established to %s", _this->_uri);
            return ESP_OK;
        }

        // WebSocket frame handler - keep connection alive
        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

        // get packet length
        esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);

        if (ret != ESP_OK)
        {
            ESP_LOGW(TAG_WEBSOCKETS, "WebSocket: Frame receive error: %s", esp_err_to_name(ret));
            return ret;
        }

        if (ws_pkt.len <= 0)
        {
            return ESP_OK;
        }

        uint8_t* buf = (uint8_t*)malloc(ws_pkt.len);
        if (!buf)
        {
            return ESP_OK;
        }

        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK)
        {
            free(buf);
            return ret;
        }

        httpd_ws_frame_t response_pkt = {0};
        response_pkt.type = HTTPD_WS_TYPE_BINARY;

        void* resource = _this->handler(ws_pkt.payload, ws_pkt.len, response_pkt.payload, response_pkt.len);
        if (resource == NULL)
        {
            return ESP_FAIL;
        }

        ret = httpd_ws_send_frame(req, &response_pkt);
        if (ret != ESP_OK)
        {
            free(buf);
            return ret;
        }

        _this->release_resource(resource);

        free(buf);
        return ESP_OK;
    }

    // return resource which needs to be released
    virtual void* handler(const uint8_t* recv_buf, size_t recv_len, uint8_t*& out_buf, size_t& out_len) = 0;

    virtual void release_resource(void* resource) {} // release resources if needed

private:
    httpd_handle_t _server_handle;
    const char* _uri;
    bool _is_registered;
};

class StreamWebsocketHandler : public WebsocketHandler
{
public:
    StreamWebsocketHandler(httpd_handle_t server_handle, const char* uri, Camera& camera)
        : WebsocketHandler(server_handle, uri), _camera(camera)
    {
    }

    virtual void* handler(const uint8_t* recv_buf, size_t recv_len, uint8_t*& out_buf, size_t& out_len)
    {
        camera_fb_t* fb = this->_camera.capture();

        if (!fb)
        {
            ESP_LOGE(TAG_WEBSOCKETS, "Frame buffer is NULL");
            return NULL;
        }

        out_buf = fb->buf;
        out_len = fb->len;

        return fb;
    }

    virtual void release_resource(void* resource)
    {
        // NOTE: this assums that the buffer is the first field of camera_fb_t
        this->_camera.release((camera_fb_t*)resource);
    }

private:
    Camera& _camera;
};