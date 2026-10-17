#include "wifi_manager.h"

#include <cstring>
#include <esp_system.h>
#include <esp_wifi.h>

static const char* TAG = "WIFI_MANAGER";

namespace
{
constexpr uint32_t kScanTimeoutMs = 20000;
constexpr uint32_t kScanCacheMs = 10000;
constexpr uint32_t kHotspotRetryMs = 5000;  // retry delay after a refused hotspot start
constexpr uint32_t kHotspotGraceMs = 10000; // keep the hotspot after a join so the phone's page can read the new IP

/** @brief RAII holder for the manager mutex (tolerates a null handle if creation failed). */
class Lock
{
public:
    explicit Lock(SemaphoreHandle_t m) : _m(m)
    {
        if (this->_m != nullptr)
        {
            xSemaphoreTake(this->_m, portMAX_DELAY);
        }
    }
    ~Lock()
    {
        if (this->_m != nullptr)
        {
            xSemaphoreGive(this->_m);
        }
    }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;

private:
    SemaphoreHandle_t _m;
};

/** @brief Overwrites the heap buffer of a String holding a password (volatile writes the optimiser cannot drop). */
void wipe_string(String& s)
{
    volatile char* p = const_cast<char*>(s.c_str());
    for (size_t i = 0; i < s.length(); ++i)
    {
        p[i] = 0;
    }
}

/** @brief Maps a disconnect reason seen while joining to a plan failure; false for reasons to ignore. */
bool classify_reason(uint8_t reason, prov::JoinFail& out)
{
    switch (reason)
    {
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        out = prov::JoinFail::WrongPassword;
        return true;
    case WIFI_REASON_NO_AP_FOUND:
        out = prov::JoinFail::NotFound;
        return true;
    case WIFI_REASON_ASSOC_FAIL:
        out = prov::JoinFail::Other;
        return true;
    default:
        // E.g. ASSOC_LEAVE from our own WiFi.begin()/disconnect(): not a failure of the new attempt.
        return false;
    }
}
} // namespace

WifiManager* WifiManager::_instance = nullptr;

void WifiManager::on_wifi_event(arduino_event_id_t event, arduino_event_info_t info)
{
    // arduino_events task: flags only, no plan calls and no logging.
    WifiManager* self = _instance;
    if (self == nullptr)
    {
        return;
    }
    if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP)
    {
        self->_ev_got_ip.store(true);
    } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
    {
        if (info.wifi_sta_disconnected.reason == WIFI_REASON_ASSOC_LEAVE)
        {
            // Our own WiFi.begin()/disconnect() (or an AP leaving): never a join failure and it must not overwrite a
            // meaningful reason. It only matters for detecting that a live connection dropped.
            self->_ev_leave.store(true);
        } else
        {
            self->_ev_reason.store(info.wifi_sta_disconnected.reason);
            self->_ev_disc.store(true);
        }
    }
}

void WifiManager::begin()
{
    this->_mutex = xSemaphoreCreateMutex();
    _instance = this;

    prov::PinRecord rec;
    this->_store.load(this->_list, rec);
    this->_pin = prov::PinGuard(rec);

    WiFi.persistent(false); // never let the Arduino core copy passwords into its own flash storage
    WiFi.onEvent(on_wifi_event);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // modem power-save batches packets/ACKs to DTIM wakeups (~100 ms), ruining stream latency
    WiFi.setAutoReconnect(false); // only enabled while Connected/Lost (see maintain())

    uint8_t mac[6] = {};
    WiFi.macAddress(mac);
    snprintf(this->_hotspot_ssid, sizeof(this->_hotspot_ssid), "VoidSentry-%02X%02X", mac[4], mac[5]);

    Lock lock(this->_mutex);
    const uint32_t now = millis();
    this->apply(this->_plan.start(this->_list.size() > 0, now), now);
}

void WifiManager::maintain()
{
    this->poll_serial();

    const uint32_t now = millis();
    {
        Lock lock(this->_mutex);
        this->process_events(now);
        if (this->_added_pending)
        {
            this->_added_pending = false;
            this->apply(this->_plan.on_network_added(now), now);
        }
        this->apply(this->_plan.tick(now), now);
        this->poll_scan(now);

        const prov::PlanState st = this->_plan.state();
        if (st != this->_prev_state)
        {
            // The core's auto-reconnect races with plan-driven joins, so it runs only while Connected/Lost.
            WiFi.setAutoReconnect(st == prov::PlanState::Connected || st == prov::PlanState::Lost);
            if (st == prov::PlanState::Setup)
            {
                // Every entry into Setup: stop the STA retrying (it would hop channels under the hotspot) and drop
                // any half-open link, so only the plan's rescans try to connect while the hotspot is up.
                WiFi.disconnect(false, false);
            }
            this->_prev_state = st;
        }

        if (this->_plan.hotspot_up() && !this->_hotspot_running && this->_hotspot_failed &&
            (int32_t)(now - this->_hotspot_retry_ms) >= 0)
        {
            this->start_hotspot(); // an earlier start was refused (no password or softAP failure): keep trying
        }

        if (this->_stop_pending)
        {
            if (this->_plan.hotspot_up())
            {
                this->_stop_pending = false; // setup mode came back before the grace period ended
            } else if ((int32_t)(now - this->_stop_at_ms) >= 0)
            {
                this->_stop_pending = false;
                this->stop_hotspot();
            }
        }
    }

    if (this->_hotspot_running)
    {
        this->_dns.processNextRequest();
    }
}

void WifiManager::process_events(uint32_t now)
{
    if (this->_ev_leave.exchange(false) && this->_plan.state() == prov::PlanState::Connected)
    {
        ESP_LOGW(TAG, "WiFi connection lost (assoc leave)");
        this->apply(this->_plan.on_disconnected(now), now);
    }
    if (this->_ev_disc.exchange(false))
    {
        const uint8_t reason = this->_ev_reason.load();
        const prov::PlanState st = this->_plan.state();
        if (st == prov::PlanState::Connected)
        {
            ESP_LOGW(TAG, "WiFi connection lost (reason %u)", (unsigned)reason);
            this->apply(this->_plan.on_disconnected(now), now);
        } else if (st == prov::PlanState::Joining)
        {
            prov::JoinFail why = prov::JoinFail::Other;
            if (classify_reason(reason, why))
            {
                ESP_LOGW(TAG, "Join of '%s' failed (reason %u)", this->_joining_ssid, (unsigned)reason);
                this->apply(this->_plan.on_join_failed(why, now), now);
            }
        }
    }

    if (this->_ev_got_ip.exchange(false))
    {
        const prov::PlanState before = this->_plan.state();
        const prov::Action a = this->_plan.on_joined(now);
        if (this->_plan.state() == prov::PlanState::Connected && before != prov::PlanState::Connected)
        {
            const String ssid = WiFi.SSID();
            const String ip = WiFi.localIP().toString();
            strlcpy(this->_cur_ssid, ssid.c_str(), sizeof(this->_cur_ssid));
            strlcpy(this->_cur_ip, ip.c_str(), sizeof(this->_cur_ip));
            ESP_LOGI(TAG, "Connected to '%s', IP %s", this->_cur_ssid, this->_cur_ip);
            if (before == prov::PlanState::Joining || before == prov::PlanState::Setup)
            {
                const int idx = this->_list.find(this->_cur_ssid, strlen(this->_cur_ssid));
                if (idx >= 0)
                {
                    this->_list.mark_joined((size_t)idx);
                    this->_store.save_nets(this->_list);
                }
            }
        }
        this->apply(a, now);
    }
}

void WifiManager::collect_scan(int count, uint32_t now)
{
    size_t n = 0;
    for (int i = 0; i < count && n < kScanCap; ++i)
    {
        const String ssid = WiFi.SSID(i);
        if (ssid.length() == 0 || ssid.length() > prov::kSsidMax)
        {
            continue; // hidden or malformed
        }
        prov::ScanResult& r = this->_scan[n++];
        r = prov::ScanResult{};
        strlcpy(r.ssid, ssid.c_str(), sizeof(r.ssid));
        r.rssi = (int8_t)constrain(WiFi.RSSI(i), -127, 0);
        r.secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    this->_scan_n = n;
    this->_scan_valid = true;
    this->_scan_time_ms = now;
}

void WifiManager::poll_scan(uint32_t now)
{
    if (this->_scan_running)
    {
        int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_RUNNING)
        {
            if ((uint32_t)(now - this->_scan_start_ms) < kScanTimeoutMs)
            {
                return;
            }
            WiFi.scanDelete();
            n = WIFI_SCAN_FAILED;
        }
        this->_scan_running = false;
        prov::Candidates cand;
        // Remember which SSID each list index meant at scan time (see apply(): the list may change before a join).
        for (size_t i = 0; i < prov::kMaxNetworks; ++i)
        {
            this->_cand_ssid[i][0] = '\0';
            if (i < this->_list.size())
            {
                strlcpy(this->_cand_ssid[i], this->_list.at(i).ssid, sizeof(this->_cand_ssid[i]));
            }
        }
        if (n >= 0)
        {
            this->collect_scan(n, now);
            WiFi.scanDelete();
            cand = prov::choose_networks(this->_scan, this->_scan_n, this->_list);
        }
        if (this->_plan_scan_pending)
        {
            this->_plan_scan_pending = false;
            this->apply(this->_plan.on_scan_done(cand, now), now);
        }
        return;
    }

    const bool ui_wanted = this->_ui_scan_wanted;
    if (this->_plan_scan_pending || ui_wanted)
    {
        this->_ui_scan_wanted = false;
        if (WiFi.scanNetworks(true) == WIFI_SCAN_FAILED)
        {
            ESP_LOGW(TAG, "Scan failed to start");
            if (this->_plan_scan_pending)
            {
                this->_plan_scan_pending = false;
                this->apply(this->_plan.on_scan_done(prov::Candidates{}, now), now);
            }
        } else
        {
            this->_scan_running = true;
            this->_scan_start_ms = now;
        }
    }
}

void WifiManager::apply(prov::Action a, uint32_t now)
{
    // A stale candidate (list edited since the scan: the index now holds another SSID or none) counts as a failed
    // join; at most 3 candidates exist.
    for (int guard = 0; guard < 4; ++guard)
    {
        switch (a.kind)
        {
        case prov::ActionKind::None:
            return;
        case prov::ActionKind::StartScan:
            this->_plan_scan_pending = true;
            return;
        case prov::ActionKind::Join:
            if (a.network >= this->_list.size() || a.network >= prov::kMaxNetworks ||
                strcmp(this->_list.at(a.network).ssid, this->_cand_ssid[a.network]) != 0)
            {
                a = this->_plan.on_join_failed(prov::JoinFail::Other, now);
                continue;
            }
            this->do_join(this->_list.at(a.network));
            return;
        case prov::ActionKind::StartHotspot:
            this->start_hotspot();
            return;
        case prov::ActionKind::StopHotspot:
            this->_stop_pending = true;
            this->_stop_at_ms = now + kHotspotGraceMs;
            return;
        }
    }
}

void WifiManager::do_join(const prov::NetworkEntry& e)
{
    strlcpy(this->_joining_ssid, e.ssid, sizeof(this->_joining_ssid));
    this->_ev_disc.store(false);
    this->_ev_got_ip.store(false);
    ESP_LOGI(TAG, "Joining '%s'", e.ssid);
    const wl_status_t st = WiFi.begin(e.ssid, e.pass[0] != '\0' ? e.pass : nullptr);
    if (st == WL_CONNECTED && WiFi.SSID().equals(e.ssid))
    {
        // Already connected to this network (e.g. by the core's auto-reconnect): begin() sends no new GOT_IP.
        this->_ev_got_ip.store(true);
    }
}

void WifiManager::start_hotspot()
{
    this->_stop_pending = false;
    if (this->_hotspot_running)
    {
        return;
    }
    String pw = this->_store.hotspot_password();
    if (pw.length() < 8) // WPA2 minimum; the store returns an empty string when NVS failed
    {
        // Never open an unprotected AP: anyone nearby could join and set the first PIN.
        ESP_LOGE(TAG, "No hotspot password available, not starting the setup hotspot (will retry)");
        wipe_string(pw);
        this->_hotspot_failed = true;
        this->_hotspot_retry_ms = millis() + kHotspotRetryMs;
        return;
    }
    WiFi.mode(WIFI_AP_STA); // AP+STA: background scans and test joins keep working while the hotspot is up
    WiFi.setSleep(false);
    if (!WiFi.softAP(this->_hotspot_ssid, pw.c_str()))
    {
        ESP_LOGE(TAG, "Failed to start the setup hotspot (will retry)");
        wipe_string(pw);
        this->_hotspot_failed = true;
        this->_hotspot_retry_ms = millis() + kHotspotRetryMs;
        return;
    }
    this->_hotspot_failed = false;
    this->_ui_scan_wanted = true; // first scan right away so the setup page has a list on its first requests
    this->_dns.setErrorReplyCode(DNSReplyCode::NoError);
    this->_dns.start(53, "*", WiFi.softAPIP());
    this->_hotspot_running = true;
    this->_hotspot_flag.store(true);
    // The hotspot password is printed here and nowhere else.
    Serial.printf("[WIFI] Setup mode. Hotspot SSID: %s  password: %s  page: http://192.168.4.1/wifi\n",
                  this->_hotspot_ssid, pw.c_str());
    wipe_string(pw);
}

void WifiManager::stop_hotspot()
{
    if (!this->_hotspot_running)
    {
        return;
    }
    this->_dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    this->_hotspot_running = false;
    this->_hotspot_flag.store(false);
    ESP_LOGI(TAG, "Setup hotspot stopped");
}

void WifiManager::poll_serial()
{
    int budget = 32; // bounded work per loop() pass
    while (budget-- > 0 && Serial.available() > 0)
    {
        const int c = Serial.read();
        if (c == '\n' || c == '\r')
        {
            const bool match =
                !this->_line_overflow && this->_line_len == 10 && memcmp(this->_line, "wifi-reset", 10) == 0;
            this->_line_len = 0;
            this->_line_overflow = false;
            if (match)
            {
                this->factory_reset();
            }
        } else if (this->_line_len < sizeof(this->_line))
        {
            this->_line[this->_line_len++] = (char)c;
        } else
        {
            this->_line_overflow = true;
        }
    }
}

void WifiManager::factory_reset()
{
    {
        Lock lock(this->_mutex);
        this->_store.erase_all();
    }
    esp_wifi_restore(); // also drop the WiFi driver's own stored STA config (older firmware used persistent(true))
    Serial.println("[WIFI] wifi-reset: saved networks and PIN erased, restarting");
    Serial.flush();
    ESP.restart();
}

const char* WifiManager::mode_name_locked() const
{
    switch (this->_plan.state())
    {
    case prov::PlanState::Connected:
        return "sta";
    case prov::PlanState::Setup:
        return "setup";
    case prov::PlanState::Lost:
        return "lost";
    default:
        return "joining";
    }
}

const char* WifiManager::mode_name()
{
    Lock lock(this->_mutex);
    return this->mode_name_locked();
}

prov::PlanState WifiManager::plan_state()
{
    Lock lock(this->_mutex);
    return this->_plan.state();
}

WifiManager::Status WifiManager::status()
{
    Status s;
    Lock lock(this->_mutex);
    const prov::PlanState st = this->_plan.state();
    s.mode = this->mode_name_locked();
    s.pin_set = this->_pin.record().set;
    if (st == prov::PlanState::Connected)
    {
        strlcpy(s.ssid, this->_cur_ssid, sizeof(s.ssid));
        strlcpy(s.ip, this->_cur_ip, sizeof(s.ip));
    } else
    {
        if (st == prov::PlanState::Joining)
        {
            strlcpy(s.ssid, this->_joining_ssid, sizeof(s.ssid));
        }
        if (this->_hotspot_flag.load())
        {
            strlcpy(s.ip, "192.168.4.1", sizeof(s.ip));
        }
        switch (this->_plan.last_failure())
        {
        case prov::JoinFail::Timeout:
            s.failure = "timeout";
            break;
        case prov::JoinFail::WrongPassword:
            s.failure = "wrong password";
            break;
        case prov::JoinFail::NotFound:
            s.failure = "not found";
            break;
        case prov::JoinFail::Other:
            s.failure = "failed";
            break;
        case prov::JoinFail::None:
            break;
        }
    }
    return s;
}

size_t WifiManager::scan(prov::ScanResult* out, size_t cap)
{
    Lock lock(this->_mutex);
    const uint32_t now = millis();
    if (!this->_scan_valid || (uint32_t)(now - this->_scan_time_ms) > kScanCacheMs)
    {
        this->_ui_scan_wanted = true; // started by maintain()
    }
    size_t n = 0;
    for (size_t i = 0; i < this->_scan_n && n < cap; ++i)
    {
        const prov::ScanResult& r = this->_scan[i];
        size_t dup = n;
        for (size_t j = 0; j < n; ++j)
        {
            if (strcmp(out[j].ssid, r.ssid) == 0)
            {
                dup = j;
                break;
            }
        }
        if (dup == n)
        {
            out[n++] = r;
        } else if (r.rssi > out[dup].rssi)
        {
            out[dup] = r;
        }
    }
    return n;
}

size_t WifiManager::saved(char (*ssids)[prov::kSsidMax + 1], size_t cap)
{
    Lock lock(this->_mutex);
    size_t n = 0;
    for (size_t i = 0; i < this->_list.size() && n < cap; ++i)
    {
        strlcpy(ssids[n++], this->_list.at(i).ssid, prov::kSsidMax + 1);
    }
    return n;
}

WifiManager::AddOutcome WifiManager::add(const char* ssid, size_t ssid_len, const char* pass, size_t pass_len,
                                         const char* pin, size_t pin_len, uint32_t now_ms)
{
    Outcome out;
    Lock lock(this->_mutex);
    if (!prov::valid_ssid(ssid, ssid_len) || !prov::valid_password(pass, pass_len))
    {
        return out; // BadInput; no PIN attempt is counted
    }

    const bool had_pin = this->_pin.record().set;
    if (!had_pin)
    {
        // A first PIN is accepted only while the hotspot is up (physical proximity) or nothing is saved yet;
        // otherwise any LAN client could take over a board whose PIN record was lost.
        if (!this->_hotspot_flag.load() && this->_list.size() > 0)
        {
            out.result = Result::WrongPin;
            return out;
        }
        uint8_t salt[prov::kSaltLen];
        esp_fill_random(salt, sizeof(salt));
        if (!this->_pin.set_pin(pin, pin_len, salt))
        {
            return out; // BadInput: the first add must carry a valid new PIN
        }
        if (!this->_store.save_pin(this->_pin.record()))
        {
            ESP_LOGE(TAG, "Could not persist the new PIN, not saving the network");
            this->_pin = prov::PinGuard(prov::PinRecord{});
            return out;
        }
    } else
    {
        const prov::PinResult r = this->_pin.check(pin, pin_len, now_ms);
        if (r.status == prov::PinCheck::LockedOut)
        {
            out.result = Result::LockedOut;
            out.seconds_left = r.seconds_left;
            return out;
        }
        if (r.status != prov::PinCheck::Ok)
        {
            out.result = Result::WrongPin;
            return out;
        }
    }

    if (this->_list.add(ssid, ssid_len, pass, pass_len) == prov::NetworkList::AddResult::Invalid)
    {
        return out;
    }
    this->_store.save_nets(this->_list);
    this->_added_pending = true;
    ESP_LOGI(TAG, "Saved network '%.*s'", (int)ssid_len, ssid);
    out.result = Result::Ok;
    return out;
}

WifiManager::ForgetOutcome WifiManager::forget(const char* ssid, size_t ssid_len, const char* pin, size_t pin_len,
                                               uint32_t now_ms)
{
    Outcome out;
    Lock lock(this->_mutex);
    if (!prov::valid_ssid(ssid, ssid_len) || !this->_pin.record().set)
    {
        return out;
    }
    const prov::PinResult r = this->_pin.check(pin, pin_len, now_ms);
    if (r.status == prov::PinCheck::LockedOut)
    {
        out.result = Result::LockedOut;
        out.seconds_left = r.seconds_left;
        return out;
    }
    if (r.status != prov::PinCheck::Ok)
    {
        out.result = Result::WrongPin;
        return out;
    }
    if (!this->_list.forget(ssid, ssid_len))
    {
        return out; // BadInput: unknown network
    }
    this->_store.save_nets(this->_list);
    ESP_LOGI(TAG, "Forgot network '%.*s'", (int)ssid_len, ssid);
    out.result = Result::Ok;
    return out;
}
