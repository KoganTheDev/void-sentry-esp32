#include "telemetry.h"

#include "windowed_counter.h"
#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <esp_freertos_hooks.h>
#include <esp_log.h>
#include <esp_timer.h>

namespace telemetry
{
namespace
{
const char* const TAG = "MAIN";

// Per-core idle share, sampled: each core's 1 kHz tick hook checks whether the task it interrupted is that core's
// IDLE task (idle0/idle1). Tasks that wake on the tick and finish within it (the motor task) are never sampled, so
// busy time is slightly under-counted. rt_idle0/rt_idle1 are the same share from the kernel's run-time counters
// (exact, but see log_stats), kept side by side so the two can be compared.
std::atomic<uint32_t> tick_samples[portNUM_PROCESSORS];
std::atomic<uint32_t> idle_samples[portNUM_PROCESSORS];

void IRAM_ATTR idle_sample_hook()
{
    const BaseType_t core = xPortGetCoreID();
    tick_samples[core].fetch_add(1, std::memory_order_relaxed);
    if (xTaskGetCurrentTaskHandleForCore(core) == xTaskGetIdleTaskHandleForCore(core))
    {
        idle_samples[core].fetch_add(1, std::memory_order_relaxed);
    }
}

// Window state of the 5 s stats line (only touched from loop()).
uint32_t last_ms = 0;
stats::WindowedCounter<uint32_t> w_copies;
stats::WindowedCounter<uint32_t> w_bytes;
stats::WindowedCounter<uint32_t> w_det_frames;
stats::WindowedCounter<uint64_t> w_decode_us;
stats::WindowedCounter<uint64_t> w_track_us;
stats::WindowedCounter<uint32_t> w_ticks[portNUM_PROCESSORS];
stats::WindowedCounter<uint32_t> w_idle[portNUM_PROCESSORS];
stats::WindowedCounter<uint32_t> w_wall_us;
stats::WindowedCounter<uint32_t> w_rt_idle[portNUM_PROCESSORS];
} // namespace

void begin()
{
    esp_register_freertos_tick_hook_for_cpu(idle_sample_hook, 0);
    esp_register_freertos_tick_hook_for_cpu(idle_sample_hook, 1);
}

void log_stats(const Sources& src)
{
    const uint32_t now = millis();
    if (now - last_ms < 5000)
    {
        return;
    }
    last_ms = now;

    const FrameStoreStats store = src.frame_store.stats();
    const DetectionStats det = src.detection.get_stats();
    const TrackSnapshot track = src.detection.get_track_snapshot();
    const uint32_t copies = src.camera_stats.copies.load();
    const uint32_t copy_us_avg = copies ? src.camera_stats.copy_us_total.load() / copies : 0;
    // Per-window JPEG size average (unsigned deltas stay correct across a bytes_total wrap).
    const uint32_t d_copies = w_copies.sample(copies);
    const uint32_t d_bytes = w_bytes.sample(src.camera_stats.bytes_total.load());
    const uint32_t jpeg_b_avg = stats::window_avg(d_bytes, d_copies);
    // Per-window detection timings (since-boot averages hid the Stage 3 slowdown).
    const uint32_t det_frames = w_det_frames.sample(det.frames);
    const uint32_t dec_win_us =
        static_cast<uint32_t>(stats::window_avg<uint64_t>(w_decode_us.sample(det.decode_us_total), det_frames));
    const uint32_t trk_win_us =
        static_cast<uint32_t>(stats::window_avg<uint64_t>(w_track_us.sample(det.track_us_total), det_frames));
    // Per-window idle % per core: sampled by the tick hook, and from the run-time counters. The ForCore getter reads
    // one task's counter under the kernel lock: no allocation, no task list walk (uxTaskGetSystemState would do both).
    // The counter source is esp_timer (microseconds, 32 bit), so a core's total time in the window is the wall time.
    const uint32_t d_wall_us = w_wall_us.sample(static_cast<uint32_t>(esp_timer_get_time()));
    unsigned idle_pct[portNUM_PROCESSORS];
    unsigned rt_idle_pct[portNUM_PROCESSORS];
    for (int core = 0; core < portNUM_PROCESSORS; ++core)
    {
        const uint32_t d_ticks = w_ticks[core].sample(tick_samples[core].load(std::memory_order_relaxed));
        const uint32_t d_idle = w_idle[core].sample(idle_samples[core].load(std::memory_order_relaxed));
        idle_pct[core] = stats::window_percent(d_idle, d_ticks);
        const uint32_t d_rt_idle = w_rt_idle[core].sample(ulTaskGetIdleRunTimeCounterForCore(core));
        rt_idle_pct[core] = stats::window_percent(d_rt_idle, d_wall_us);
    }
    const aiming::Angles pose = src.turret.position_deg();
    const ControlStatus ctl = src.controller.control_status();
    ESP_LOGI(
        TAG,
        "[stream] up=%us captured=%u sent=%u dropped_no_slot=%u oversize=%u corrupt=%u viewers=%u "
        "copy_us_avg=%u heap=%u heap_min=%u psram=%u psram_min=%u "
        "decode_us_avg=%u track_us_avg=%u scene_changes=%u decode_fail=%u track=%s id=%u mode=%s pan=%.1f tilt=%.1f "
        "steps_s=%.0f motor_late=%u motor_stack_free=%u aims=%u jpeg_b_avg=%u",
        (unsigned)(now / 1000), (unsigned)src.camera_stats.captured.load(), (unsigned)src.http_server.frames_sent(),
        (unsigned)store.dropped_no_slot, (unsigned)store.dropped_oversize, (unsigned)store.dropped_corrupt,
        (unsigned)src.http_server.viewer_count(), (unsigned)copy_us_avg, (unsigned)ESP.getFreeHeap(),
        (unsigned)ESP.getMinFreeHeap(), (unsigned)ESP.getFreePsram(), (unsigned)ESP.getMinFreePsram(),
        (unsigned)det.decode_us_avg, (unsigned)det.track_us_avg, (unsigned)det.scene_changes,
        (unsigned)det.decode_failures, tracking::to_string(track.state), (unsigned)track.id, to_string(ctl.mode),
        (double)pose.pan, (double)pose.tilt, (double)src.turret.pan_speed(), (unsigned)src.turret.late_ticks(),
        (unsigned)src.turret.stack_free(), (unsigned)ctl.aims, (unsigned)jpeg_b_avg);
    // Second line: the ROM printf behind Arduino logging truncates the first one after jpeg_b_avg.
    ESP_LOGI(TAG,
             "[detect] det_frames=%u dec_win_us=%u trk_win_us=%u det_stack_free=%u idle0=%u idle1=%u rssi=%d wifi=%s "
             "rt_idle0=%u rt_idle1=%u",
             (unsigned)det_frames, (unsigned)dec_win_us, (unsigned)trk_win_us, (unsigned)det.stack_free, idle_pct[0],
             idle_pct[1], (int)WiFi.RSSI(), src.wifi.mode_name(), rt_idle_pct[0], rt_idle_pct[1]);
}
} // namespace telemetry
