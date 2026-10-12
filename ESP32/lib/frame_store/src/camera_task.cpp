#include "camera_task.h"

#include <cstring>
#include <esp_camera.h>
#include <esp_log.h>
#include <esp_timer.h>

#include "jpeg_check.h"

static const char* TAG_CAMERA_TASK = "CAMERA_TASK";

namespace
{
struct CameraTaskContext {
    FrameStore* store = nullptr;
    CameraTaskStats* stats = nullptr;
};

CameraTaskContext g_context;

void camera_task(void*)
{
    for (;;)
    {
        camera_fb_t* fb = esp_camera_fb_get();
        if (fb == nullptr)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        g_context.stats->captured++;

        std::optional<size_t> slot = g_context.store->begin_write();
        if (slot)
        {
            if (fb->len > FrameStore::kSlotBytes)
            {
                g_context.store->abort_write(*slot, frames::AbortReason::Oversize);
            } else if (!frames::is_complete_jpeg(fb->buf, fb->len))
            {
                g_context.store->abort_write(*slot, frames::AbortReason::Corrupt);
            } else
            {
                const int64_t start_us = esp_timer_get_time();
                std::memcpy(g_context.store->write_buffer(*slot), fb->buf, fb->len);
                g_context.stats->copy_us_total += static_cast<uint32_t>(esp_timer_get_time() - start_us);
                g_context.stats->copies++;

                frames::FrameMeta meta;
                meta.length = fb->len;
                meta.width = static_cast<uint16_t>(fb->width);
                meta.height = static_cast<uint16_t>(fb->height);
                meta.timestamp_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
                g_context.store->publish(*slot, meta);
            }
        }
        esp_camera_fb_return(fb); // hand the driver buffer back right away
    }
}
} // namespace

bool start_camera_task(FrameStore& store, CameraTaskStats& stats)
{
    g_context.store = &store;
    g_context.stats = &stats;
    if (xTaskCreatePinnedToCore(camera_task, "camera", 4096, nullptr, 2, nullptr, 1) != pdPASS)
    {
        ESP_LOGE(TAG_CAMERA_TASK, "Failed to create camera task");
        return false;
    }
    return true;
}
