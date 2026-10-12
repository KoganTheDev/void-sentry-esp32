#include "stream_broadcaster.h"

#include <esp_log.h>
#include <unistd.h>

static const char* TAG_STREAM = "STREAM";

StreamBroadcaster* StreamBroadcaster::instance_ = nullptr;

namespace
{
class Lock
{
public:
    explicit Lock(SemaphoreHandle_t mutex) : mutex_(mutex) { xSemaphoreTake(mutex_, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(mutex_); }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;

private:
    SemaphoreHandle_t mutex_;
};
} // namespace

bool StreamBroadcaster::start(httpd_handle_t server, FrameStore& store)
{
    server_ = server;
    store_ = &store;
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr)
    {
        ESP_LOGE(TAG_STREAM, "Failed to create mutex");
        return false;
    }
    instance_ = this;

    httpd_uri_t uri = {};
    uri.uri = "/stream";
    uri.method = HTTP_GET;
    uri.handler = ws_handler;
    uri.user_ctx = this;
    uri.is_websocket = true;
    if (httpd_register_uri_handler(server_, &uri) != ESP_OK)
    {
        ESP_LOGE(TAG_STREAM, "Failed to register /stream");
        return false;
    }

    TaskHandle_t task = nullptr;
    if (xTaskCreatePinnedToCore(stream_task, "stream", 4096, this, 1, &task, 1) != pdPASS)
    {
        ESP_LOGE(TAG_STREAM, "Failed to create stream task");
        return false;
    }
    store_->set_subscriber(task);
    return true;
}

size_t StreamBroadcaster::viewer_count()
{
    Lock lock(mutex_);
    return viewers_.size();
}

esp_err_t StreamBroadcaster::ws_handler(httpd_req_t* req)
{
    auto* self = static_cast<StreamBroadcaster*>(req->user_ctx);
    const int fd = httpd_req_to_sockfd(req);

    if (req->method == HTTP_GET) // WebSocket handshake completed
    {
        bool added = false;
        {
            Lock lock(self->mutex_);
            added = self->viewers_.add(fd);
        }
        if (!added)
        {
            ESP_LOGW(TAG_STREAM, "Viewer limit (%u) reached, closing fd %d", (unsigned)frames::kMaxViewers, fd);
            httpd_sess_trigger_close(req->handle, fd);
        } else
        {
            ESP_LOGI(TAG_STREAM, "Viewer connected (fd %d)", fd);
        }
        return ESP_OK;
    }

    // The board pushes; clients have nothing to say on /stream. Drain and ignore small frames.
    httpd_ws_frame_t pkt = {};
    esp_err_t err = httpd_ws_recv_frame(req, &pkt, 0);
    if (err != ESP_OK)
    {
        return err;
    }
    if (pkt.len > 0)
    {
        uint8_t scratch[64];
        if (pkt.len > sizeof(scratch))
        {
            return ESP_FAIL; // unexpected large client message: let the server close the session
        }
        pkt.payload = scratch;
        err = httpd_ws_recv_frame(req, &pkt, pkt.len);
    }
    return err;
}

void StreamBroadcaster::on_session_closed(httpd_handle_t, int sockfd)
{
    if (instance_ != nullptr)
    {
        instance_->remove_viewer(sockfd);
    }
    close(sockfd);
}

void StreamBroadcaster::remove_viewer(int fd)
{
    Lock lock(mutex_);
    if (viewers_.index_of(fd))
    {
        viewers_.remove(fd);
        ESP_LOGI(TAG_STREAM, "Viewer disconnected (fd %d)", fd);
    }
}

void StreamBroadcaster::stream_task(void* arg) { static_cast<StreamBroadcaster*>(arg)->run(); }

void StreamBroadcaster::run()
{
    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)); // woken by FrameStore::publish

        frames::ReadyList ready;
        {
            std::optional<frames::FrameLease> probe = store_->acquire_latest();
            if (!probe)
            {
                continue;
            }
            Lock lock(mutex_);
            ready = viewers_.ready_for(probe->meta().sequence);
        } // probe released here, outside the broadcaster mutex

        for (size_t i = 0; i < ready.count; ++i)
        {
            const int fd = ready.fds[i];
            std::optional<frames::FrameLease> lease = store_->acquire_latest();
            if (!lease)
            {
                break;
            }

            SendJob* job = nullptr;
            {
                Lock lock(mutex_);
                std::optional<size_t> index = viewers_.index_of(fd);
                if (!index || jobs_[*index].in_flight)
                {
                    continue; // viewer left, or its previous send is still queued
                }
                job = &jobs_[*index];
                job->in_flight = true;
                viewers_.mark_sending(fd, lease->meta().sequence);
            }

            job->owner = this;
            job->fd = fd;
            job->lease = std::move(lease);
            if (httpd_queue_work(server_, send_job, job) != ESP_OK)
            {
                job->lease.reset();
                Lock lock(mutex_);
                viewers_.mark_done(fd);
                job->in_flight = false;
            }
        }
    }
}

void StreamBroadcaster::send_job(void* arg)
{
    auto* job = static_cast<SendJob*>(arg);
    StreamBroadcaster* self = job->owner;

    httpd_ws_frame_t frame = {};
    frame.final = true;
    frame.type = HTTPD_WS_TYPE_BINARY;
    frame.payload = const_cast<uint8_t*>(self->store_->data(*job->lease));
    frame.len = job->lease->meta().length;

    const esp_err_t err = httpd_ws_send_frame_async(self->server_, job->fd, &frame);
    job->lease.reset(); // release the slot before touching the broadcaster mutex

    // Known, accepted race: if this viewer's socket closed and the OS reused the same fd for a brand-new connection
    // while this job was still queued, a failed send here can remove and close the new viewer instead of the old one.
    // This is rare and harmless: the new viewer just reconnects. close_fn (on_session_closed) removes the viewer on
    // every close, so the set never holds a stale fd for long.
    const int fd = job->fd;
    {
        Lock lock(self->mutex_);
        if (err == ESP_OK)
        {
            self->viewers_.mark_done(fd);
        } else
        {
            self->viewers_.remove(fd);
        }
        job->in_flight = false;
    }

    if (err == ESP_OK)
    {
        self->sent_++;
    } else
    {
        ESP_LOGW(TAG_STREAM, "Send to fd %d failed (%s), dropping viewer", fd, esp_err_to_name(err));
        httpd_sess_trigger_close(self->server_, fd);
    }
}
