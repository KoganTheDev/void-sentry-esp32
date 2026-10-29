#include "telemetry.h"

#include "windowed_counter.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_log.h>
#include <esp_timer.h>

#if !CONFIG_FREERTOS_RUN_TIME_STATS_USING_ESP_TIMER
// cppcheck-suppress preprocessorErrorDirective
#error "idle0/idle1 assume microsecond run-time counters: needs CONFIG_FREERTOS_RUN_TIME_STATS_USING_ESP_TIMER=y"
#endif

namespace telemetry
{
namespace
{
const char* const TAG = "MAIN";

// Window state of the 5 s stats line (only touched from loop()).
uint32_t last_ms = 0;
stats::WindowedCounter<uint32_t> w_copies;
stats::WindowedCounter<uint32_t> w_bytes;
stats::WindowedCounter<uint32_t> w_det_frames;
stats::WindowedCounter<uint64_t> w_decode_us;
stats::WindowedCounter<uint64_t> w_track_us;
stats::WindowedCounter<uint32_t> w_wall_us;
stats::WindowedCounter<uint32_t> w_idle_us[portNUM_PROCESSORS];
stats::WindowedCounter<uint32_t> w_sent;
stats::WindowedCounter<uint64_t> w_send_us;
stats::WindowedCounter<uint32_t> w_send_slow;
stats::WindowedCounter<uint32_t> w_send_failed;
} // namespace

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
    // Per-window idle % per core from the kernel's run-time counters (how core 0 saturation shows before the task
    // watchdog, which watches IDLE0 only, reboots the board). The ForCore getter reads one task's counter under the
    // kernel lock: no allocation, no task list walk (uxTaskGetSystemState would do both). The counter source is
    // esp_timer (microseconds, 32 bit, unsigned deltas survive the wrap), so a core's total time in the window is the
    // wall time. This replaced a 1 kHz tick-hook sampler; both agreed within 2 points on the board (Stage 8, A2).
    const uint32_t d_wall_us = w_wall_us.sample(static_cast<uint32_t>(esp_timer_get_time()));
    unsigned idle_pct[portNUM_PROCESSORS];
    for (int core = 0; core < portNUM_PROCESSORS; ++core)
    {
        const uint32_t d_idle_us = w_idle_us[core].sample(ulTaskGetIdleRunTimeCounterForCore(core));
        idle_pct[core] = stats::window_percent(d_idle_us, d_wall_us);
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
             "[detect] det_frames=%u dec_win_us=%u trk_win_us=%u det_stack_free=%u idle0=%u idle1=%u rssi=%d wifi=%s",
             (unsigned)det_frames, (unsigned)dec_win_us, (unsigned)trk_win_us, (unsigned)det.stack_free, idle_pct[0],
             idle_pct[1], (int)WiFi.RSSI(), src.wifi.mode_name());
    // Third line: how long /stream sends block the httpd task, which also serves /commands (a long send freezes the
    // video for every viewer and delays HUD replies). snd_busy is the share of the window spent inside sends.
    const StreamBroadcaster::SendStats snd = src.http_server.take_send_stats();
    const uint32_t d_sent = w_sent.sample(src.http_server.frames_sent());
    const uint64_t d_send_us = w_send_us.sample(snd.total_us);
    ESP_LOGI(TAG, "[net] sent_win=%u snd_avg_us=%u snd_max_ms=%u snd_slow=%u snd_fail=%u snd_busy=%u", (unsigned)d_sent,
             (unsigned)stats::window_avg<uint64_t>(d_send_us, d_sent), (unsigned)(snd.max_us / 1000),
             (unsigned)w_send_slow.sample(snd.slow), (unsigned)w_send_failed.sample(snd.failed),
             stats::window_percent(static_cast<uint32_t>(d_send_us), d_wall_us));
}
} // namespace telemetry
