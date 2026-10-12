#include "camera_diff_detection.h"

#include "direction.h"
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
    const int sx = s.frame_width / tracking::kGridW; // 4 for QVGA
    const int sy = s.frame_height / tracking::kGridH;
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
    void* mem = heap_caps_malloc(sizeof(Pipeline), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!mem)
    {
        ESP_LOGE(TAG, "Failed to allocate the %u-byte tracking pipeline in PSRAM", (unsigned)sizeof(Pipeline));
        return false;
    }
    this->_pipeline = new (mem) Pipeline{};
    ESP_LOGI(TAG, "Tracking pipeline ready (%u bytes in PSRAM)", (unsigned)sizeof(Pipeline));
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
        portENTER_CRITICAL(&this->_lock);
        ++this->_stats.decode_failures;
        portEXIT_CRITICAL(&this->_lock);
        ESP_LOGD(TAG, "JPEG decode failed (%u bytes)", (unsigned)frame.length);
        return no_move;
    }

    const tracking::DetectionResult& detection = this->_pipeline->detector.process(this->_pipeline->gray);
    const tracking::TrackState& track = this->_pipeline->tracker.update(detection);
    const int64_t t2 = esp_timer_get_time();

    const TrackSnapshot snapshot = make_snapshot(track, frame.width, frame.height);
    portENTER_CRITICAL(&this->_lock);
    this->_snapshot = snapshot;
    ++this->_stats.frames;
    if (detection.scene_change)
    {
        ++this->_stats.scene_changes;
    }
    this->_decode_us_total += static_cast<uint64_t>(t1 - t0);
    this->_track_us_total += static_cast<uint64_t>(t2 - t1);
    this->_stats.decode_us_avg = static_cast<uint32_t>(this->_decode_us_total / this->_stats.frames);
    this->_stats.track_us_avg = static_cast<uint32_t>(this->_track_us_total / this->_stats.frames);
    portEXIT_CRITICAL(&this->_lock);

    return tracking::direction_for(track);
}

TrackSnapshot CameraDiffDetection::get_track_snapshot() const
{
    portENTER_CRITICAL(&this->_lock);
    const TrackSnapshot copy = this->_snapshot;
    portEXIT_CRITICAL(&this->_lock);
    return copy;
}

DetectionStats CameraDiffDetection::get_stats() const
{
    portENTER_CRITICAL(&this->_lock);
    const DetectionStats copy = this->_stats;
    portEXIT_CRITICAL(&this->_lock);
    return copy;
}
