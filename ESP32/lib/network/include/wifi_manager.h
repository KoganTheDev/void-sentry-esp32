/**
 * @file wifi_manager.h
 * @brief Non-blocking WiFi provisioning: saved networks, setup hotspot, PIN-protected changes.
 */

#pragma once

#include "choose.h"
#include "form.h"
#include "network_list.h"
#include "pin_guard.h"
#include "prov_types.h"
#include "sync.h"
#include "wifi_plan.h"
#include "wifi_store.h"
#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/**
 * @class WifiManager
 * @brief Glue between the pure prov::WifiPlan and the Arduino WiFi stack.
 * @details Driven only from loop() through maintain(), which never waits for a join or scan to complete (scans are
 * async, WiFi events (arduino_events task) only set atomic flags, and maintain() turns them into plan events). All WiFi
 * API calls and plan calls happen in maintain()/begin(), and maintain() holds one FreeRTOS mutex for its whole pass,
 * including WiFi driver calls that take a while (WiFi.mode/softAP when the hotspot starts or stops, WiFi.begin,
 * starting a scan) and NVS writes (save_nets after a join).
 *
 * status() (behind /api/wifi/status, which the HUD polls) never takes that mutex: maintain(), begin() and add() publish
 * a copy of its fields into a spinlock-guarded snapshot whenever they may have changed, and status() copies that. The
 * rarer, user-initiated accessors (scan(), saved(), add(), forget()) still share the network list, PIN guard and scan
 * cache with maintain() under the mutex, so a call that lands during one of those slow driver calls waits for it, and
 * the single httpd task (every /stream send and /commands reply) waits with it. add() and forget() also hold the mutex
 * across their own NVS writes, which blocks loop() in maintain() for that time.
 */
class WifiManager
{
public:
    /** @brief Snapshot for /api/wifi/status. */
    struct Status {
        const char* mode = "joining"; ///< "sta" | "setup" | "joining" | "lost" (string literal)
        char ssid[prov::kSsidMax + 1] = {};
        char ip[16] = {};
        bool pin_set = false;
        const char* failure = ""; ///< "wrong password" | "not found" | "timeout" | "failed" | "" (string literal)
    };

    /** @brief Result of add()/forget(); the HTTP layer maps it to 200 / 400 / 403 / 429. */
    enum class Result { Ok, BadInput, WrongPin, LockedOut };

    /** @brief Outcome with the lockout time for Result::LockedOut. */
    struct Outcome {
        Result result = Result::BadInput;
        uint32_t seconds_left = 0;
    };
    using AddOutcome = Outcome;
    using ForgetOutcome = Outcome;

    /** @brief Capacity of the scan cache (strongest networks beyond it are not guaranteed to be kept). */
    static constexpr size_t kScanCap = 24;

    WifiManager() = default;
    WifiManager(const WifiManager&) = delete;
    WifiManager& operator=(const WifiManager&) = delete;

    /**
     * @brief Loads NVS, brings up the WiFi stack and starts the connection plan. Returns at once.
     * @details Must run before the HTTP server starts (it sets up the TCP/IP stack).
     */
    void begin();

    /**
     * @brief Call from loop(): WiFi events, scan polling, plan timers, hotspot DNS and the serial `wifi-reset`.
     */
    void maintain();

    /**
     * @brief Snapshot of mode, SSID, IP, whether a PIN is set and the last failure.
     * @details Never takes the manager mutex (one short spinlock copy), so it cannot wait behind maintain(). The copy
     * is the one published at the end of the last maintain() pass (or by begin()/add()), so it lags the live state
     * by at most one pass. It holds no password or PIN.
     */
    Status status();

    /** @brief Short mode name for logs ("sta" | "setup" | "joining" | "lost"). */
    const char* mode_name();

    /** @brief true while the setup hotspot is running (atomic, no lock). */
    bool setup_mode() const { return this->_hotspot_flag.load(); }

    /** @brief Current plan state (thread-safe; for tests and diagnostics). */
    prov::PlanState plan_state();

    /**
     * @brief Copies the cached scan (strongest entry per SSID, hidden networks skipped).
     * @details Requests a new async scan from maintain() if the cache is older than 10 s, so the first call after a
     * long pause may return an old or empty list; call again after a few seconds.
     * @return Number of entries written to out (at most cap).
     */
    size_t scan(prov::ScanResult* out, size_t cap);

    /** @brief Copies the saved SSIDs. @return Number written (at most cap). */
    size_t saved(char (*ssids)[prov::kSsidMax + 1], size_t cap);

    /**
     * @brief Adds (or replaces) a network after checking the PIN; the first add with no PIN set defines the PIN.
     * @details Inputs are validated before any PIN attempt is counted. Persists the list (and the new PIN record) and
     * asks maintain() to tell the plan; a full list evicts the oldest entry.
     */
    AddOutcome add(const char* ssid, size_t ssid_len, const char* pass, size_t pass_len, const char* pin,
                   size_t pin_len, uint32_t now_ms);

    /** @brief Forgets a saved network after checking the PIN (same PinGuard as add, so lockouts are shared). */
    ForgetOutcome forget(const char* ssid, size_t ssid_len, const char* pin, size_t pin_len, uint32_t now_ms);

private:
    static void on_wifi_event(arduino_event_id_t event, arduino_event_info_t info);

    void process_events(uint32_t now);
    void poll_scan(uint32_t now);
    void collect_scan(int count, uint32_t now);
    void apply(prov::Action a, uint32_t now);
    void do_join(const prov::NetworkEntry& e);
    void start_hotspot();
    void stop_hotspot();
    void poll_serial();
    void factory_reset();
    const char* mode_name_locked() const;
    /** @brief Builds the /api/wifi/status fields from the live state. Caller holds _mutex. */
    Status build_status_locked() const;
    /** @brief Publishes build_status_locked() for status(). Caller holds _mutex. */
    void publish_status_locked();

    static WifiManager* _instance;

    SemaphoreHandle_t _mutex = nullptr;
    WifiStore _store;
    prov::NetworkList _list;
    prov::PinGuard _pin;
    prov::WifiPlan _plan;
    DNSServer _dns;

    // Event flags written by the arduino_events task.
    std::atomic<bool> _ev_got_ip{false};
    std::atomic<bool> _ev_disc{false};
    std::atomic<bool> _ev_leave{false}; // ASSOC_LEAVE: kept apart from _ev_reason, see on_wifi_event()
    std::atomic<uint8_t> _ev_reason{0};
    std::atomic<bool> _hotspot_flag{false};

    /** @brief Copy of the status() fields, written under _mutex by publish_status_locked(), read lock-free. */
    vsync::SpinGuarded<Status> _status_snap;

    // Shared with the HTTP layer (guarded by _mutex).
    prov::ScanResult _scan[kScanCap];
    size_t _scan_n = 0;
    bool _scan_valid = false;
    uint32_t _scan_time_ms = 0;
    bool _ui_scan_wanted = false;
    bool _added_pending = false;
    char _cur_ssid[prov::kSsidMax + 1] = {};
    char _cur_ip[16] = {};
    char _joining_ssid[prov::kSsidMax + 1] = {};

    // Used by the loop() side only.
    bool _scan_running = false;
    bool _plan_scan_pending = false;
    uint32_t _scan_start_ms = 0;
    prov::PlanState _prev_state = prov::PlanState::Boot;
    char _cand_ssid[prov::kMaxNetworks][prov::kSsidMax + 1] = {}; // list SSIDs at the last scan, by list index
    bool _hotspot_failed = false;
    uint32_t _hotspot_retry_ms = 0;
    bool _hotspot_running = false;
    bool _stop_pending = false;
    uint32_t _stop_at_ms = 0;
    char _hotspot_ssid[20] = {};
    char _line[24] = {};
    size_t _line_len = 0;
    bool _line_overflow = false;
};
