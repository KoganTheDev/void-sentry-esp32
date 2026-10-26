/**
 * @file stream_broadcaster.h
 * @brief Pushes the latest camera frame to each idle /stream WebSocket viewer (drop-when-busy, low latency).
 * @details Sends run inside the HTTP server's own task via httpd_queue_work + httpd_ws_send_frame_async, the
 * thread-safe way to use esp_http_server from another task. Each viewer has one preallocated send job, so the
 * frame path allocates nothing. A viewer still receiving the previous frame is skipped, never queued.
 */

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "frame_slots.h"
#include "frame_store.h"
#include "viewer_set.h"

class StreamBroadcaster
{
public:
    /** @brief Register the /stream WebSocket endpoint and start the stream task (core 1, priority 4). */
    bool start(httpd_handle_t server, FrameStore& store);

    /**
     * @brief Stop the stream task and release all resources. Call BEFORE httpd_stop. Safe to call repeatedly.
     */
    void stop();

    /** @brief httpd close_fn: forget the viewer and close the socket (required when close_fn is set). */
    static void on_session_closed(httpd_handle_t server, int sockfd);

    uint32_t frames_sent() const { return sent_.load(); }

    /**
     * @brief Connected /stream viewers (0 to frames::kMaxViewers).
     * @details Safe from any task, including the httpd task (the /commands reply): it takes the broadcaster mutex,
     * which every holder keeps for a few microseconds only and never while calling back into this method, so the
     * caller cannot deadlock and waits at most that long (the FreeRTOS mutex has priority inheritance). Returns 0
     * before start() and after stop().
     */
    size_t viewer_count() const;

private:
    struct SendJob {
        StreamBroadcaster* owner = nullptr;
        int fd = -1;
        size_t index = 0;        ///< viewer slot this job was claimed for
        uint32_t generation = 0; ///< generation_[index] at claim time
        bool in_flight = false;
        std::optional<frames::FrameLease> lease;
    };

    static esp_err_t ws_handler(httpd_req_t* req);
    static esp_err_t pre_handshake(httpd_req_t* req);  // ws_pre_handshake_cb: origin check, before the upgrade
    static esp_err_t post_handshake(httpd_req_t* req); // registers the viewer once the session is a WebSocket
    static void stream_task(void* arg);
    static void send_job(void* arg);
    void run();
    void remove_viewer(int fd);
    void wake_task(); // notify the stream task that a send finished

    httpd_handle_t server_ = nullptr;
    FrameStore* store_ = nullptr;
    frames::ViewerSet viewers_;
    std::array<SendJob, frames::kMaxViewers> jobs_{};
    std::array<uint32_t, frames::kMaxViewers> generation_{}; // bumped per new viewer in a slot; guards viewers_
    TaskHandle_t task_ = nullptr;
    SemaphoreHandle_t mutex_ = nullptr; // guards viewers_, generation_ and jobs_[i].in_flight
    std::atomic<uint32_t> sent_{0};

    static StreamBroadcaster* instance_; // close_fn has no user context
};
