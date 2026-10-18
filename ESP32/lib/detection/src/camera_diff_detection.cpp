#include "camera_diff_detection.h"

#include "direction.h"
#include "frame_store.h"
#include "jpeg_gray.h"

#include <algorithm>
#include <esp32-hal-log.h> // without it this file's ESP_LOGx output is dropped
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <new>

static const char* TAG = "DETECTION";

namespace
{
TrackSnapshot make_snapshot(const tracking::TrackState& track, size_t frame_width, size_t frame_height)
{
    TrackSnapshot s;
    s.frame_width = static_cast<int>(frame_width);
    s.frame_height = static_cast<int>(frame_height);
    s.state = track.state;
    s.id = track.id;
    if (track.state == tracking::TrackPhase::Idle)
    {
        return s;
    }
    const int sx = s.frame_width / tracking::kGridW;  // 8 for QVGA (320 / 40)
    const int sy = s.frame_height / tracking::kGridH; // 8 for QVGA (240 / 30)
    if (sx == 0 || sy == 0)
    {
        return s; // frame smaller than the grid: report the Idle coordinates instead of a zero-sized scale
    }
    s.x = std::clamp(static_cast<int>(track.cx * sx + sx / 2), 0, s.frame_width - 1);
    s.y = std::clamp(static_cast<int>(track.cy * sy + sy / 2), 0, s.frame_height - 1);
    const int x0 = std::clamp(track.box.x0 * sx, 0, s.frame_width);
    const int y0 = std::clamp(track.box.y0 * sy, 0, s.frame_height);
    const int x1 = std::clamp((track.box.x1 + 1) * sx, 0, s.frame_width);
    const int y1 = std::clamp((track.box.y1 + 1) * sy, 0, s.frame_height);
    s.bx = x0;
    s.by = y0;
    s.bw = x1 - x0;
    s.bh = y1 - y0;
    s.vx = track.vx * static_cast<float>(sx);
    s.vy = track.vy * static_cast<float>(sy);
    s.area_px = track.area * sx * sy;
    return s;
}
} // namespace

CameraDiffDetection::~CameraDiffDetection()
{
    if (this->_pipeline)
    {
        this->_pipeline->~Pipeline();
        heap_caps_free(this->_pipeline);
    }
}

bool CameraDiffDetection::begin()
{
    if (this->_pipeline)
    {
        return true;
    }
    void* mem = heap_caps_malloc(sizeof(Pipeline), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!mem)
    {
        ESP_LOGE(TAG, "Failed to allocate the %u-byte tracking pipeline in internal RAM", (unsigned)sizeof(Pipeline));
        return false;
    }
    this->_pipeline = new (mem) Pipeline{};
    ESP_LOGI(TAG, "Tracking pipeline ready (%u bytes in internal RAM)", (unsigned)sizeof(Pipeline));
    return true;
}

std::tuple<MoveX, MoveY> CameraDiffDetection::detect_object(camera_buffer_t frame)
{
    const auto no_move = std::make_tuple(MoveX(MoveX::None), MoveY(MoveY::None));
    if (!this->_pipeline || !frame.buffer || frame.length == 0)
    {
        return no_move;
    }

    const int64_t t0 = esp_timer_get_time();
    const bool decoded =
        jpeg_gray::decode(static_cast<const uint8_t*>(frame.buffer), frame.length, this->_pipeline->gray);
    const int64_t t1 = esp_timer_get_time();
    if (!decoded)
    {
        // A frame we cannot read counts as a miss, so the track decays instead of freezing the old snapshot.
        const tracking::TrackState& missed = this->_pipeline->tracker.update(tracking::DetectionResult{});
        const TrackSnapshot snapshot = make_snapshot(missed, frame.width, frame.height);
        this->_published.update([&](Published& p) {
            ++p.stats.decode_failures;
            p.snapshot = snapshot;
        });
        ESP_LOGD(TAG, "JPEG decode failed (%u bytes)", (unsigned)frame.length);
        return tracking::direction_for(missed);
    }

    const tracking::DetectionResult& detection = this->_pipeline->detector.process(this->_pipeline->gray);
    const tracking::TrackState& track = this->_pipeline->tracker.update(detection);
    const int64_t t2 = esp_timer_get_time();

    const TrackSnapshot snapshot = make_snapshot(track, frame.width, frame.height);
    this->_published.update([&](Published& p) {
        DetectionStats& stats = p.stats;
        p.snapshot = snapshot;
        ++stats.frames;
        if (detection.scene_change)
        {
            ++stats.scene_changes;
        }
        stats.decode_us_total += static_cast<uint64_t>(t1 - t0);
        stats.track_us_total += static_cast<uint64_t>(t2 - t1);
        stats.decode_us_avg = static_cast<uint32_t>(stats.decode_us_total / stats.frames);
        stats.track_us_avg = static_cast<uint32_t>(stats.track_us_total / stats.frames);
    });

    return tracking::direction_for(track);
}

bool CameraDiffDetection::start_task(FrameStore& store)
{
    if (this->_task != nullptr)
    {
        return true;
    }
    if (!this->_pipeline)
    {
        ESP_LOGE(TAG, "start_task() called before begin()");
        return false;
    }
    this->_store = &store;
    TaskHandle_t task = nullptr;
    if (xTaskCreatePinnedToCore(task_entry, "detect", 4096, this, 3, &task, 1) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create the detection task");
        return false;
    }
    if (!store.add_subscriber(task))
    {
        ESP_LOGE(TAG, "FrameStore subscriber slots are full");
        vTaskDelete(task);
        return false;
    }
    this->_task = task;
    return true;
}

void CameraDiffDetection::task_entry(void* arg) { static_cast<CameraDiffDetection*>(arg)->task_loop(); }

void CameraDiffDetection::task_loop()
{
    uint32_t processed = 0;
    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200)); // woken by FrameStore::publish; timeout serves reset requests

        if (this->_reset_requested.exchange(false))
        {
            this->apply_reset();
        }

        std::optional<frames::FrameLease> lease = this->_store->acquire_latest();
        if (!lease || lease->meta().sequence == this->_last_sequence)
        {
            continue;
        }
        this->_last_sequence = lease->meta().sequence;

        camera_buffer_t fb{const_cast<uint8_t*>(this->_store->data(*lease)), lease->meta().length, lease->meta().width,
                           lease->meta().height};
        this->detect_object(fb); // the result is read back as a track snapshot
        lease.reset();

        if (++processed % 100 == 0)
        {
            // StackType_t is uint8_t on ESP-IDF, so the high-water mark is in bytes.
            const uint32_t free_bytes = static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr));
            this->_published.update([&](Published& p) { p.stats.stack_free = free_bytes; });
        }
    }
}

void CameraDiffDetection::apply_reset()
{
    this->_pipeline->detector.reset();
    this->_pipeline->tracker.reset();
    this->_published.update([](Published& p) {
        TrackSnapshot idle;
        idle.frame_width = p.snapshot.frame_width;
        idle.frame_height = p.snapshot.frame_height;
        p.snapshot = idle;
    });
}

void CameraDiffDetection::reset()
{
    this->_reset_requested.store(true);
    // Wake the detect task (priority 3) so it preempts the caller on core 1, applies the reset and publishes Idle
    // before this returns; otherwise a caller in the same loop() pass could still read a stale Locked snapshot.
    if (this->_task)
    {
        xTaskNotifyGive(this->_task);
    }
}

TrackSnapshot CameraDiffDetection::get_track_snapshot() const
{
    return this->_published.inspect([](const Published& p) { return p.snapshot; });
}

DetectionStats CameraDiffDetection::get_stats() const
{
    return this->_published.inspect([](const Published& p) { return p.stats; });
}
