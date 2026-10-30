#include "stream_broadcaster.h"

#include "handshake_guard.h"
#include "sync.h"

#include <esp32-hal-log.h> // route ESP_LOGx through Arduino's logger (otherwise silenced)
#include <esp_log.h>
#include <esp_timer.h>
#include <lwip/sockets.h>
#include <unistd.h>

static const char* TAG_STREAM = "STREAM";

StreamBroadcaster* StreamBroadcaster::instance_ = nullptr;

bool StreamBroadcaster::start(httpd_handle_t server, FrameStore& store)
{
    if (task_ != nullptr)
    {
        return true; // already started
    }
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
    uri.ws_pre_handshake_cb = pre_handshake;
    uri.ws_post_handshake_cb = post_handshake;
    if (httpd_register_uri_handler(server_, &uri) != ESP_OK)
    {
        ESP_LOGE(TAG_STREAM, "Failed to register /stream");
        stop();
        return false;
    }

    // Priority 4: the work per wake is tens of microseconds (lease + queue the send). At priority 1 it would wait
    // behind a whole detection (priority 3) on every frame, adding video latency.
    task_ = store_->start_subscriber_task(stream_task, "stream", 4096, 4, 1, this);
    if (task_ == nullptr)
    {
        stop();
        return false;
    }
    return true;
}

void StreamBroadcaster::stop()
{
    if (store_ != nullptr && task_ != nullptr)
    {
        store_->remove_subscriber(task_); // before deleting the task, so publish never notifies a dead handle
    }
    if (task_ != nullptr)
    {
        vTaskDelete(task_);
        task_ = nullptr;
    }
    if (instance_ == this)
    {
        instance_ = nullptr;
    }
    if (mutex_ != nullptr)
    {
        SemaphoreHandle_t mutex = mutex_;
        mutex_ = nullptr;
        vSemaphoreDelete(mutex);
    }
    // Task is gone and the mutex too: drop any leases still parked in queued jobs (the server is about to stop).
    for (SendJob& job : jobs_)
    {
        job.lease.reset();
        job.in_flight = false;
    }
    viewers_ = frames::ViewerSet{};
}

StreamBroadcaster::SendStats StreamBroadcaster::take_send_stats()
{
    return SendStats{.total_us = send_us_total_.load(),
                     .slow = send_slow_.load(),
                     .failed = send_failed_.load(),
                     .max_us = send_us_max_.exchange(0)};
}

size_t StreamBroadcaster::viewer_count() const
{
    if (mutex_ == nullptr)
    {
        return 0;
    }
    vsync::MutexLock lock(mutex_);
    return viewers_.size();
}

esp_err_t StreamBroadcaster::pre_handshake(httpd_req_t* req)
{
    // Any port: the page is on 80, /stream on 81. A refused viewer is never added.
    return handshake_origin_ok(req, TAG_STREAM, true) ? ESP_OK : ESP_FAIL;
}

esp_err_t StreamBroadcaster::post_handshake(httpd_req_t* req)
{
    auto* self = static_cast<StreamBroadcaster*>(req->user_ctx);
    const int fd = httpd_req_to_sockfd(req);
    bool added = false;
    {
        vsync::MutexLock lock(self->mutex_);
        const bool is_new = !self->viewers_.index_of(fd);
        added = self->viewers_.add(fd);
        if (added && is_new)
        {
            if (auto index = self->viewers_.index_of(fd))
            {
                ++self->generation_[*index]; // identifies this viewer, not just its (reusable) fd
            }
        }
    }
    if (!added)
    {
        ESP_LOGW(TAG_STREAM, "Viewer limit (%u) reached, closing fd %d", (unsigned)frames::kMaxViewers, fd);
        httpd_sess_trigger_close(req->handle, fd);
    } else
    {
        // httpd leaves Nagle on. A frame goes out as a 4-byte header send then the payload, and Nagle holds each
        // frame's last partial segment until the previous data is ACKed, which the viewer may delay (delayed ACK).
        const int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        ESP_LOGI(TAG_STREAM, "Viewer connected (fd %d)", fd);
    }
    return ESP_OK;
}

esp_err_t StreamBroadcaster::ws_handler(httpd_req_t* req)
{
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
    if (mutex_ == nullptr)
    {
        return;
    }
    bool removed = false;
    {
        vsync::MutexLock lock(mutex_);
        if (viewers_.index_of(fd))
        {
            viewers_.remove(fd);
            removed = true;
        }
    } // log outside the lock: serial output is slow and would stall every task waiting on the broadcaster mutex
    if (removed)
    {
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
            vsync::MutexLock lock(mutex_);
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
                vsync::MutexLock lock(mutex_);
                std::optional<size_t> index = viewers_.index_of(fd);
                if (!index || jobs_[*index].in_flight)
                {
                    continue; // viewer left, or its previous send is still queued
                }
                job = &jobs_[*index];
                job->in_flight = true;
                job->index = *index;
                job->generation = generation_[*index];
                viewers_.mark_sending(fd, lease->meta().sequence);
            }

            job->owner = this;
            job->fd = fd;
            job->lease = std::move(lease);
            if (httpd_queue_work(server_, send_job, job) != ESP_OK)
            {
                job->lease.reset();
                vsync::MutexLock lock(mutex_);
                viewers_.mark_done(fd);
                job->in_flight = false;
            }
        }
    }
}

void StreamBroadcaster::wake_task()
{
    if (task_ != nullptr)
    {
        xTaskNotifyGive(task_);
    }
}

void StreamBroadcaster::send_job(void* arg)
{
    auto* job = static_cast<SendJob*>(arg);
    StreamBroadcaster* self = job->owner;

    const int fd = job->fd;

    if (self->mutex_ == nullptr) // broadcaster was stopped while this job was queued
    {
        job->lease.reset();
        job->in_flight = false;
        return;
    }

    // A queued job can outlive its viewer, and the OS may reuse the fd for an unrelated session. So the job carries
    // the viewer's slot index and generation; it only acts on the fd if that exact viewer is still registered.
    // The check is exact because post_handshake (add), close_fn (remove) and this job all run on the httpd task, so no
    // close/add can interleave between the check and the send below.
    auto still_same_viewer = [&]() {
        vsync::MutexLock lock(self->mutex_);
        const std::optional<size_t> index = self->viewers_.index_of(fd);
        return index && *index == job->index && self->generation_[job->index] == job->generation;
    };

    if (!still_same_viewer())
    {
        job->lease.reset(); // release the slot before touching the broadcaster mutex
        {
            vsync::MutexLock lock(self->mutex_);
            job->in_flight = false;
        }
        self->wake_task();
        return; // viewer is gone: send nothing, close nothing
    }

    httpd_ws_frame_t frame = {};
    frame.final = true;
    frame.type = HTTPD_WS_TYPE_BINARY;
    frame.payload = const_cast<uint8_t*>(self->store_->data(*job->lease));
    frame.len = job->lease->meta().length;

    const int64_t send_start_us = esp_timer_get_time();
    const esp_err_t err = httpd_ws_send_frame_async(self->server_, fd, &frame);
    const auto send_us = static_cast<uint32_t>(esp_timer_get_time() - send_start_us);
    job->lease.reset(); // release the slot before touching the broadcaster mutex

    // This task raises the max while take_send_stats() (stats line, loop task) resets it with exchange(0): a CAS loop
    // keeps a concurrent reset from being overwritten by a stale larger value.
    self->send_us_total_.fetch_add(send_us);
    uint32_t cur = self->send_us_max_.load();
    while (send_us > cur && !self->send_us_max_.compare_exchange_weak(cur, send_us))
    {
    }
    if (send_us >= kSlowSendUs)
    {
        self->send_slow_.fetch_add(1);
    }
    if (err != ESP_OK)
    {
        self->send_failed_.fetch_add(1);
    }

    bool matched = false;
    {
        vsync::MutexLock lock(self->mutex_);
        const std::optional<size_t> index = self->viewers_.index_of(fd);
        matched = index && *index == job->index && self->generation_[job->index] == job->generation;
        if (matched)
        {
            if (err == ESP_OK)
            {
                self->viewers_.mark_done(fd);
            } else
            {
                self->viewers_.remove(fd);
            }
        }
        job->in_flight = false;
    }

    // Wake the stream task so a viewer that just finished gets the newest frame now, not at the next publish.
    // ready_for() already prevents sending the same sequence twice.
    self->wake_task();

    if (err == ESP_OK)
    {
        self->sent_++;
    } else if (matched)
    {
        // ESP_ERR_INVALID_ARG means the session is already gone: expected during disconnects, not worth a warning.
        if (err == ESP_ERR_INVALID_ARG)
        {
            ESP_LOGD(TAG_STREAM, "Send to fd %d failed (session gone), dropping viewer", fd);
        } else
        {
            ESP_LOGW(TAG_STREAM, "Send to fd %d failed (%s), dropping viewer", fd, esp_err_to_name(err));
        }
        httpd_sess_trigger_close(self->server_, fd);
    }
}
