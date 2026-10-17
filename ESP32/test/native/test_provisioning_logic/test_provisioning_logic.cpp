/**
 * @file test_provisioning_logic.cpp
 * @brief Host-side tests for SHA-256, PinGuard, form decoding and the WifiPlan state machine.
 */

#include <unity.h>

#include "form.h"
#include "pin_guard.h"
#include "sha256.h"
#include "wifi_plan.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

using namespace prov;

void setUp(void) {}
void tearDown(void) {}

// ---- helpers ----

static void expect_digest(const char* hex, const std::array<uint8_t, 32>& d)
{
    char got[65];
    for (size_t i = 0; i < 32; ++i)
    {
        std::snprintf(got + i * 2, 3, "%02x", d[i]);
    }
    TEST_ASSERT_EQUAL_STRING(hex, got);
}

static std::array<uint8_t, 32> sha_str(const char* s)
{
    return sha256(reinterpret_cast<const uint8_t*>(s), std::strlen(s));
}

// ---- SHA-256 ----

void test_sha256_empty(void)
{
    expect_digest("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", sha256(nullptr, 0));
}

void test_sha256_abc(void)
{
    expect_digest("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", sha_str("abc"));
}

void test_sha256_448_bits(void)
{
    expect_digest("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
                  sha_str("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"));
}

static uint8_t g_million[1000000];

void test_sha256_million_a(void)
{
    std::memset(g_million, 'a', sizeof(g_million));
    expect_digest("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
                  sha256(g_million, sizeof(g_million)));
}

void test_sha256_padding_boundaries_differ(void)
{
    // 55, 56, 63, 64 byte inputs hit every padding branch; they must all be distinct and stable.
    uint8_t buf[65];
    std::memset(buf, 'x', sizeof(buf));
    const size_t lens[] = {55, 56, 63, 64, 65};
    std::array<uint8_t, 32> prev{};
    for (size_t l : lens)
    {
        const auto d = sha256(buf, l);
        TEST_ASSERT_TRUE(d != prev);
        prev = d;
    }
}

// ---- PinGuard ----

static const uint8_t kSalt[kSaltLen] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

static PinGuard make_guard(const char* pin = "1234")
{
    PinGuard g;
    TEST_ASSERT_TRUE(g.set_pin(pin, std::strlen(pin), kSalt));
    return g;
}

static PinResult chk(PinGuard& g, const char* pin, uint32_t now) { return g.check(pin, std::strlen(pin), now); }

// Five wrong PINs at time t; returns the result of the fifth.
static PinResult five_wrong(PinGuard& g, uint32_t t)
{
    PinResult r;
    for (int i = 0; i < 5; ++i)
    {
        r = chk(g, "0000", t);
    }
    return r;
}

void test_pin_not_set(void)
{
    PinGuard g;
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::NotSet), static_cast<int>(chk(g, "1234", 0).status));
    TEST_ASSERT_FALSE(g.record().set);
}

void test_pin_set_rejects_invalid(void)
{
    PinGuard g;
    TEST_ASSERT_FALSE(g.set_pin("123", 3, kSalt));
    TEST_ASSERT_FALSE(g.set_pin("123456789", 9, kSalt));
    TEST_ASSERT_FALSE(g.set_pin("12a4", 4, kSalt));
    TEST_ASSERT_FALSE(g.set_pin(nullptr, 4, kSalt));
    TEST_ASSERT_FALSE(g.record().set);
}

void test_pin_ok_and_wrong(void)
{
    PinGuard g = make_guard();
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Ok), static_cast<int>(chk(g, "1234", 0).status));
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Wrong), static_cast<int>(chk(g, "1235", 0).status));
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Wrong), static_cast<int>(chk(g, "12345", 0).status));
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Wrong), static_cast<int>(chk(g, "abcd", 0).status));
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Wrong), static_cast<int>(g.check(nullptr, 0, 0).status));
}

void test_pin_hash_is_salted_sha256(void)
{
    PinGuard g = make_guard("1234");
    uint8_t buf[kSaltLen + 4];
    std::memcpy(buf, kSalt, kSaltLen);
    std::memcpy(buf + kSaltLen, "1234", 4);
    const auto h = sha256(buf, sizeof(buf));
    TEST_ASSERT_TRUE(g.record().set);
    TEST_ASSERT_EQUAL_MEMORY(kSalt, g.record().salt, kSaltLen);
    TEST_ASSERT_EQUAL_MEMORY(h.data(), g.record().hash, kHashLen);
}

void test_pin_four_wrong_then_ok_resets(void)
{
    PinGuard g = make_guard();
    for (int i = 0; i < 4; ++i)
    {
        TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Wrong), static_cast<int>(chk(g, "0000", 10).status));
    }
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Ok), static_cast<int>(chk(g, "1234", 10).status));
    // The counter restarted: four more wrongs still do not lock.
    for (int i = 0; i < 4; ++i)
    {
        TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Wrong), static_cast<int>(chk(g, "0000", 11).status));
    }
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Ok), static_cast<int>(chk(g, "1234", 11).status));
}

void test_pin_first_lockout_60s_boundaries(void)
{
    PinGuard g = make_guard();
    const uint32_t t0 = 1000;
    const PinResult fifth = five_wrong(g, t0);
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Wrong), static_cast<int>(fifth.status));
    PinResult r = chk(g, "1234", t0);
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::LockedOut), static_cast<int>(r.status));
    TEST_ASSERT_EQUAL_UINT32(60, r.seconds_left);
    r = chk(g, "1234", t0 + 59500); // even the right PIN is refused, 0.5 s left rounds up to 1
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::LockedOut), static_cast<int>(r.status));
    TEST_ASSERT_EQUAL_UINT32(1, r.seconds_left);
    r = chk(g, "1234", t0 + 59999);
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::LockedOut), static_cast<int>(r.status));
    r = chk(g, "1234", t0 + 60000);
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Ok), static_cast<int>(r.status));
}

void test_pin_locked_state_does_not_evaluate(void)
{
    PinGuard g = make_guard();
    five_wrong(g, 0);
    // Hammering while locked must not extend or escalate the lockout.
    for (int i = 0; i < 20; ++i)
    {
        TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::LockedOut), static_cast<int>(chk(g, "0000", 1000).status));
    }
    const PinResult r = chk(g, "1234", 60000);
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Ok), static_cast<int>(r.status));
}

void test_pin_second_lockout_120s(void)
{
    PinGuard g = make_guard();
    five_wrong(g, 0);
    const uint32_t t1 = 60000;
    // After the first lockout the failure count restarted from zero: four wrongs do not lock.
    for (int i = 0; i < 4; ++i)
    {
        TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Wrong), static_cast<int>(chk(g, "0000", t1).status));
    }
    chk(g, "0000", t1); // fifth
    PinResult r = chk(g, "1234", t1);
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::LockedOut), static_cast<int>(r.status));
    TEST_ASSERT_EQUAL_UINT32(120, r.seconds_left);
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::LockedOut), static_cast<int>(chk(g, "1234", t1 + 119999).status));
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Ok), static_cast<int>(chk(g, "1234", t1 + 120000).status));
}

void test_pin_lockout_caps_at_3600s(void)
{
    PinGuard g = make_guard();
    uint32_t t = 0;
    uint32_t expect = 60;
    for (int k = 0; k < 10; ++k)
    {
        five_wrong(g, t);
        const PinResult r = chk(g, "1234", t);
        TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::LockedOut), static_cast<int>(r.status));
        TEST_ASSERT_EQUAL_UINT32(expect, r.seconds_left);
        t += expect * 1000u;
        expect = (expect * 2 > 3600) ? 3600 : expect * 2;
    }
    TEST_ASSERT_EQUAL_UINT32(3600, expect);
}

void test_pin_ok_after_lockout_resets_k(void)
{
    PinGuard g = make_guard();
    five_wrong(g, 0);
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Ok), static_cast<int>(chk(g, "1234", 60000).status));
    five_wrong(g, 70000);
    const PinResult r = chk(g, "1234", 70000);
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::LockedOut), static_cast<int>(r.status));
    TEST_ASSERT_EQUAL_UINT32(60, r.seconds_left); // k was reset, so 60 s again
}

void test_pin_lockout_wrap_safe(void)
{
    PinGuard g = make_guard();
    const uint32_t t0 = UINT32_MAX - 10000u; // lockout spans the uint32 wrap
    five_wrong(g, t0);
    PinResult r = chk(g, "1234", t0 + 30000u);
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::LockedOut), static_cast<int>(r.status));
    TEST_ASSERT_EQUAL_UINT32(30, r.seconds_left);
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Ok), static_cast<int>(chk(g, "1234", t0 + 60000u).status));
}

void test_pin_failures_accumulate_across_callers(void)
{
    // Review focus 5: one guard serves add and forget; failures from alternating callers add up.
    PinGuard g = make_guard();
    for (int i = 0; i < 4; ++i)
    {
        const char* who_pin = (i % 2 == 0) ? "1111" : "2222"; // "add" then "forget"
        TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Wrong), static_cast<int>(chk(g, who_pin, 5).status));
    }
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Wrong), static_cast<int>(chk(g, "3333", 5).status));
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::LockedOut), static_cast<int>(chk(g, "1234", 5).status));
}

void test_pin_record_round_trip(void)
{
    PinGuard a = make_guard("98765432");
    PinGuard b(a.record());
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Ok), static_cast<int>(chk(b, "98765432", 0).status));
    TEST_ASSERT_EQUAL(static_cast<int>(PinCheck::Wrong), static_cast<int>(chk(b, "98765431", 0).status));
}

// ---- form ----

static bool field(const char* body, const char* key, char* out, size_t cap, size_t* n = nullptr)
{
    return form_field(body, std::strlen(body), key, out, cap, n);
}

void test_form_plain_field(void)
{
    char out[40];
    size_t n = 99;
    TEST_ASSERT_TRUE(field("ssid=Home&password=hunter22&pin=1234", "password", out, sizeof(out), &n));
    TEST_ASSERT_EQUAL_STRING("hunter22", out);
    TEST_ASSERT_EQUAL_size_t(8, n);
    TEST_ASSERT_TRUE(field("ssid=Home&password=hunter22&pin=1234", "ssid", out, sizeof(out), &n));
    TEST_ASSERT_EQUAL_STRING("Home", out);
    TEST_ASSERT_TRUE(field("ssid=Home&password=hunter22&pin=1234", "pin", out, sizeof(out), &n));
    TEST_ASSERT_EQUAL_STRING("1234", out);
}

void test_form_empty_value(void)
{
    char out[8] = "zzz";
    size_t n = 99;
    TEST_ASSERT_TRUE(field("ssid=Home&password=&pin=1", "password", out, sizeof(out), &n));
    TEST_ASSERT_EQUAL_size_t(0, n);
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_form_plus_and_percent(void)
{
    char out[40];
    size_t n = 0;
    TEST_ASSERT_TRUE(field("ssid=My+Net", "ssid", out, sizeof(out), &n));
    TEST_ASSERT_EQUAL_STRING("My Net", out);
    TEST_ASSERT_TRUE(field("password=a%26b", "password", out, sizeof(out), &n));
    TEST_ASSERT_EQUAL_STRING("a&b", out);
    TEST_ASSERT_TRUE(field("password=p%26ss%3D%2B%25+x", "password", out, sizeof(out), &n));
    TEST_ASSERT_EQUAL_STRING("p&ss=+% x", out);
    TEST_ASSERT_EQUAL_size_t(9, n);
    TEST_ASSERT_TRUE(field("password=%4a%4A", "password", out, sizeof(out), &n)); // either hex case
    TEST_ASSERT_EQUAL_STRING("JJ", out);
}

void test_form_utf8(void)
{
    char out[40];
    size_t n = 0;
    TEST_ASSERT_TRUE(field("ssid=%D7%A9", "ssid", out, sizeof(out), &n));
    TEST_ASSERT_EQUAL_size_t(2, n);
    TEST_ASSERT_EQUAL_UINT8(0xD7, static_cast<uint8_t>(out[0]));
    TEST_ASSERT_EQUAL_UINT8(0xA9, static_cast<uint8_t>(out[1]));
    TEST_ASSERT_EQUAL_UINT8(0, static_cast<uint8_t>(out[2]));
}

void test_form_key_prefix_not_confused(void)
{
    char out[40];
    TEST_ASSERT_TRUE(field("xssid=wrong&ssid=right", "ssid", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("right", out);
    TEST_ASSERT_FALSE(field("xssid=wrong", "ssid", out, sizeof(out)));
    TEST_ASSERT_FALSE(field("ssidx=wrong", "ssid", out, sizeof(out)));
    TEST_ASSERT_FALSE(field("ssid", "ssid", out, sizeof(out))); // no '=' at all
}

void test_form_missing_key(void)
{
    char out[40];
    TEST_ASSERT_FALSE(field("a=1&b=2", "c", out, sizeof(out)));
    TEST_ASSERT_FALSE(field("", "c", out, sizeof(out)));
}

void test_form_malformed_escapes(void)
{
    char out[40];
    TEST_ASSERT_FALSE(field("ssid=ab%4", "ssid", out, sizeof(out)));
    TEST_ASSERT_FALSE(field("ssid=ab%", "ssid", out, sizeof(out)));
    TEST_ASSERT_FALSE(field("ssid=%G1", "ssid", out, sizeof(out)));
    TEST_ASSERT_FALSE(field("ssid=%1G", "ssid", out, sizeof(out)));
    TEST_ASSERT_FALSE(field("ssid=a%00b", "ssid", out, sizeof(out)));    // decoded NUL
    TEST_ASSERT_FALSE(field("ssid=%4&pin=1", "ssid", out, sizeof(out))); // escape cut by '&'
    TEST_ASSERT_TRUE(field("ssid=%41", "ssid", out, sizeof(out)));       // exactly at the end is fine
    TEST_ASSERT_EQUAL_STRING("A", out);
}

void test_form_body_size_limit(void)
{
    char body[300];
    std::memset(body, 'a', sizeof(body));
    std::memcpy(body, "k=", 2);
    char out[300];
    TEST_ASSERT_TRUE(form_field(body, 256, "k", out, sizeof(out), nullptr));
    TEST_ASSERT_FALSE(form_field(body, 257, "k", out, sizeof(out), nullptr));
}

void test_form_out_cap(void)
{
    char out[6];
    size_t n = 0;
    TEST_ASSERT_TRUE(field("k=abcde", "k", out, 6, &n)); // 5 chars + NUL fills 6 exactly
    TEST_ASSERT_EQUAL_STRING("abcde", out);
    TEST_ASSERT_FALSE(field("k=abcdef", "k", out, 6, &n));
    TEST_ASSERT_FALSE(field("k=a", "k", out, 0, &n));
    TEST_ASSERT_FALSE(field("k=%41%41%41%41%41%41", "k", out, 6, &n)); // limit applies to decoded size
    TEST_ASSERT_TRUE(field("k=%41%41%41%41%41", "k", out, 6, &n));
}

void test_validate_ssid(void)
{
    char s[40];
    std::memset(s, 'a', sizeof(s));
    TEST_ASSERT_FALSE(valid_ssid(s, 0));
    TEST_ASSERT_TRUE(valid_ssid(s, 1));
    TEST_ASSERT_TRUE(valid_ssid(s, 32));
    TEST_ASSERT_FALSE(valid_ssid(s, 33));
    TEST_ASSERT_FALSE(valid_ssid(nullptr, 5));
}

void test_validate_password(void)
{
    char s[70];
    std::memset(s, 'a', sizeof(s));
    TEST_ASSERT_TRUE(valid_password(s, 0));
    TEST_ASSERT_TRUE(valid_password(nullptr, 0));
    TEST_ASSERT_FALSE(valid_password(s, 7));
    TEST_ASSERT_TRUE(valid_password(s, 8));
    TEST_ASSERT_TRUE(valid_password(s, 63));
    TEST_ASSERT_FALSE(valid_password(s, 64));
    s[3] = '\x07';
    TEST_ASSERT_FALSE(valid_password(s, 8));
    s[3] = static_cast<char>(0x7F);
    TEST_ASSERT_FALSE(valid_password(s, 8));
    s[3] = static_cast<char>(0xD7);
    TEST_ASSERT_FALSE(valid_password(s, 8));
    s[3] = ' ';
    TEST_ASSERT_TRUE(valid_password(s, 8));
    s[3] = '~';
    TEST_ASSERT_TRUE(valid_password(s, 8));
}

void test_validate_pin(void)
{
    TEST_ASSERT_FALSE(valid_pin("123", 3));
    TEST_ASSERT_TRUE(valid_pin("1234", 4));
    TEST_ASSERT_TRUE(valid_pin("12345678", 8));
    TEST_ASSERT_FALSE(valid_pin("123456789", 9));
    TEST_ASSERT_FALSE(valid_pin("12a4", 4));
    TEST_ASSERT_FALSE(valid_pin(nullptr, 4));
}

// ---- WifiPlan ----

static Candidates cands(std::initializer_list<uint8_t> idx)
{
    Candidates c;
    for (uint8_t i : idx)
    {
        c.idx[c.count++] = i;
    }
    return c;
}

static void expect_action(ActionKind k, const Action& a)
{
    TEST_ASSERT_EQUAL(static_cast<int>(k), static_cast<int>(a.kind));
}
static void expect_state(PlanState s, const WifiPlan& p)
{
    TEST_ASSERT_EQUAL(static_cast<int>(s), static_cast<int>(p.state()));
}

// Plan driven to Joining with candidates {2, 0}; join started at t.
static WifiPlan joining_plan(uint32_t t)
{
    WifiPlan p;
    p.start(true, t);
    const Action a = p.on_scan_done(cands({2, 0}), t);
    TEST_ASSERT_EQUAL(static_cast<int>(ActionKind::Join), static_cast<int>(a.kind));
    TEST_ASSERT_EQUAL_UINT8(2, a.network);
    return p;
}

static WifiPlan connected_plan(void)
{
    WifiPlan p = joining_plan(0);
    p.on_joined(100);
    return p;
}

static WifiPlan setup_plan(uint32_t t)
{
    WifiPlan p;
    p.start(false, t);
    return p;
}

void test_plan_start_with_saved(void)
{
    WifiPlan p;
    expect_state(PlanState::Boot, p);
    expect_action(ActionKind::StartScan, p.start(true, 0));
    expect_state(PlanState::Scanning, p);
    TEST_ASSERT_FALSE(p.hotspot_up());
}

void test_plan_start_without_saved(void)
{
    WifiPlan p;
    expect_action(ActionKind::StartHotspot, p.start(false, 0));
    expect_state(PlanState::Setup, p);
    TEST_ASSERT_TRUE(p.hotspot_up());
}

void test_plan_start_twice_ignored(void)
{
    WifiPlan p;
    p.start(true, 0);
    expect_action(ActionKind::None, p.start(false, 5));
    expect_state(PlanState::Scanning, p);
    TEST_ASSERT_FALSE(p.hotspot_up());
}

void test_plan_scan_with_candidates_joins_first(void)
{
    WifiPlan p;
    p.start(true, 0);
    const Action a = p.on_scan_done(cands({1, 2}), 50);
    expect_action(ActionKind::Join, a);
    TEST_ASSERT_EQUAL_UINT8(1, a.network);
    expect_state(PlanState::Joining, p);
}

void test_plan_scan_without_candidates_goes_to_setup(void)
{
    WifiPlan p;
    p.start(true, 0);
    expect_action(ActionKind::StartHotspot, p.on_scan_done(Candidates{}, 50));
    expect_state(PlanState::Setup, p);
    TEST_ASSERT_TRUE(p.hotspot_up());
}

void test_plan_joined_connects_without_hotspot(void)
{
    WifiPlan p = joining_plan(0);
    expect_action(ActionKind::None, p.on_joined(1000));
    expect_state(PlanState::Connected, p);
}

void test_plan_join_failure_tries_next_candidate(void)
{
    WifiPlan p = joining_plan(0);
    const Action a = p.on_join_failed(JoinFail::WrongPassword, 2000);
    expect_action(ActionKind::Join, a);
    TEST_ASSERT_EQUAL_UINT8(0, a.network);
    expect_state(PlanState::Joining, p);
    TEST_ASSERT_EQUAL(static_cast<int>(JoinFail::WrongPassword), static_cast<int>(p.last_failure()));
}

void test_plan_all_candidates_fail_goes_to_setup(void)
{
    WifiPlan p = joining_plan(0);
    p.on_join_failed(JoinFail::NotFound, 1000);
    expect_action(ActionKind::StartHotspot, p.on_join_failed(JoinFail::Other, 2000));
    expect_state(PlanState::Setup, p);
    TEST_ASSERT_TRUE(p.hotspot_up());
    TEST_ASSERT_EQUAL(static_cast<int>(JoinFail::Other), static_cast<int>(p.last_failure()));
}

void test_plan_join_timeout_exact(void)
{
    WifiPlan p = joining_plan(1000);
    expect_action(ActionKind::None, p.tick(1000 + 14999));
    expect_state(PlanState::Joining, p);
    const Action a = p.tick(1000 + 15000);
    expect_action(ActionKind::Join, a); // next candidate
    TEST_ASSERT_EQUAL_UINT8(0, a.network);
    TEST_ASSERT_EQUAL(static_cast<int>(JoinFail::Timeout), static_cast<int>(p.last_failure()));
    // The second candidate gets a fresh 15 s.
    expect_action(ActionKind::None, p.tick(1000 + 15000 + 14999));
    expect_action(ActionKind::StartHotspot, p.tick(1000 + 15000 + 15000));
    expect_state(PlanState::Setup, p);
}

void test_plan_join_timeout_wrap_safe(void)
{
    const uint32_t t = UINT32_MAX - 5000u;
    WifiPlan p = joining_plan(t);
    expect_action(ActionKind::None, p.tick(t + 14999u));
    expect_action(ActionKind::Join, p.tick(t + 15000u));
}

void test_plan_disconnect_goes_lost_then_setup_at_120s(void)
{
    WifiPlan p = connected_plan();
    expect_action(ActionKind::None, p.on_disconnected(10000));
    expect_state(PlanState::Lost, p);
    expect_action(ActionKind::None, p.tick(10000 + 119999));
    expect_state(PlanState::Lost, p);
    expect_action(ActionKind::StartHotspot, p.tick(10000 + 120000));
    expect_state(PlanState::Setup, p);
    TEST_ASSERT_TRUE(p.hotspot_up());
}

void test_plan_lost_timer_wrap_safe(void)
{
    WifiPlan p = connected_plan();
    const uint32_t t = UINT32_MAX - 1000u;
    p.on_disconnected(t);
    expect_action(ActionKind::None, p.tick(t + 119999u));
    expect_action(ActionKind::StartHotspot, p.tick(t + 120000u));
}

void test_plan_lost_reconnect(void)
{
    WifiPlan p = connected_plan();
    p.on_disconnected(10000);
    expect_action(ActionKind::None, p.on_joined(12000));
    expect_state(PlanState::Connected, p);
    TEST_ASSERT_FALSE(p.hotspot_up());
}

void test_plan_joined_from_setup_join_stops_hotspot(void)
{
    WifiPlan p = setup_plan(0);
    p.on_network_added(1000);
    expect_action(ActionKind::Join, p.on_scan_done(cands({1}), 2000));
    expect_state(PlanState::Joining, p);
    TEST_ASSERT_TRUE(p.hotspot_up());
    expect_action(ActionKind::StopHotspot, p.on_joined(5000));
    expect_state(PlanState::Connected, p);
    TEST_ASSERT_FALSE(p.hotspot_up());
}

void test_plan_setup_rescan_every_30s(void)
{
    WifiPlan p = setup_plan(1000);
    expect_action(ActionKind::None, p.tick(1000 + 29999));
    expect_action(ActionKind::StartScan, p.tick(1000 + 30000));
    expect_state(PlanState::Setup, p);
    TEST_ASSERT_TRUE(p.hotspot_up());
    // Next rescan 30 s after the last one.
    expect_action(ActionKind::None, p.tick(1000 + 30000 + 29999));
    expect_action(ActionKind::StartScan, p.tick(1000 + 60000));
}

void test_plan_setup_rescan_wrap_safe(void)
{
    const uint32_t t = UINT32_MAX - 100u;
    WifiPlan p = setup_plan(t);
    expect_action(ActionKind::None, p.tick(t + 29999u));
    expect_action(ActionKind::StartScan, p.tick(t + 30000u));
}

void test_plan_setup_scan_finds_saved_network(void)
{
    WifiPlan p = setup_plan(0);
    p.tick(30000);
    const Action a = p.on_scan_done(cands({1}), 30500);
    expect_action(ActionKind::Join, a);
    TEST_ASSERT_EQUAL_UINT8(1, a.network);
    expect_state(PlanState::Joining, p);
    TEST_ASSERT_TRUE(p.hotspot_up());
}

void test_plan_setup_empty_scan_changes_nothing(void)
{
    WifiPlan p = setup_plan(0);
    expect_action(ActionKind::None, p.on_scan_done(Candidates{}, 100));
    expect_state(PlanState::Setup, p);
    TEST_ASSERT_TRUE(p.hotspot_up());
}

void test_plan_network_added_in_setup_scans(void)
{
    WifiPlan p = setup_plan(0);
    expect_action(ActionKind::StartScan, p.on_network_added(10));
    expect_state(PlanState::Setup, p);
}

void test_plan_network_added_in_lost_scans(void)
{
    WifiPlan p = connected_plan();
    p.on_disconnected(1000);
    expect_action(ActionKind::StartScan, p.on_network_added(2000));
    expect_state(PlanState::Lost, p);
    expect_action(ActionKind::Join, p.on_scan_done(cands({0}), 2500));
    expect_state(PlanState::Joining, p);
}

void test_plan_network_added_while_connected_is_none(void)
{
    // Review focus 2: storing a network must not disturb a live connection.
    WifiPlan p = connected_plan();
    expect_action(ActionKind::None, p.on_network_added(5000));
    expect_state(PlanState::Connected, p);
    expect_action(ActionKind::None, p.tick(5000 + 1000000));
    expect_state(PlanState::Connected, p);
}

void test_plan_network_added_in_other_states_is_none(void)
{
    WifiPlan boot;
    expect_action(ActionKind::None, boot.on_network_added(0));
    expect_state(PlanState::Boot, boot);

    WifiPlan scanning;
    scanning.start(true, 0);
    expect_action(ActionKind::None, scanning.on_network_added(1));
    expect_state(PlanState::Scanning, scanning);

    WifiPlan joining = joining_plan(0);
    expect_action(ActionKind::None, joining.on_network_added(1));
    expect_state(PlanState::Joining, joining);
}

void test_plan_failed_setup_join_keeps_hotspot(void)
{
    // Review focus 4: after a failed join from setup the hotspot stays up, with no second StartHotspot.
    WifiPlan p = setup_plan(0);
    p.on_network_added(1000);
    expect_action(ActionKind::Join, p.on_scan_done(cands({0}), 2000));
    expect_action(ActionKind::None, p.on_join_failed(JoinFail::WrongPassword, 4000));
    expect_state(PlanState::Setup, p);
    TEST_ASSERT_TRUE(p.hotspot_up());
    TEST_ASSERT_EQUAL(static_cast<int>(JoinFail::WrongPassword), static_cast<int>(p.last_failure()));
    // Setup keeps rescanning afterwards.
    expect_action(ActionKind::None, p.tick(4000 + 29999));
    expect_action(ActionKind::StartScan, p.tick(4000 + 30000));
}

void test_plan_failed_setup_join_by_timeout_keeps_hotspot(void)
{
    WifiPlan p = setup_plan(0);
    p.on_scan_done(cands({0}), 1000);
    expect_action(ActionKind::None, p.tick(1000 + 15000));
    expect_state(PlanState::Setup, p);
    TEST_ASSERT_TRUE(p.hotspot_up());
    TEST_ASSERT_EQUAL(static_cast<int>(JoinFail::Timeout), static_cast<int>(p.last_failure()));
}

void test_plan_inapplicable_events_do_nothing(void)
{
    WifiPlan p;
    expect_action(ActionKind::None, p.on_scan_done(cands({0}), 0));
    expect_action(ActionKind::None, p.on_joined(0));
    expect_action(ActionKind::None, p.on_join_failed(JoinFail::Other, 0));
    expect_action(ActionKind::None, p.on_disconnected(0));
    expect_action(ActionKind::None, p.tick(1000000));
    expect_state(PlanState::Boot, p);
    TEST_ASSERT_EQUAL(static_cast<int>(JoinFail::None), static_cast<int>(p.last_failure()));

    WifiPlan c = connected_plan();
    expect_action(ActionKind::None, c.on_scan_done(cands({1}), 10));
    expect_action(ActionKind::None, c.on_join_failed(JoinFail::Other, 10));
    expect_action(ActionKind::None, c.on_joined(10));
    expect_state(PlanState::Connected, c);

    WifiPlan j = joining_plan(0);
    expect_action(ActionKind::None, j.on_disconnected(10));
    expect_action(ActionKind::None, j.on_scan_done(cands({1}), 10));
    expect_state(PlanState::Joining, j);

    WifiPlan s = setup_plan(0);
    expect_action(ActionKind::None, s.on_joined(10));
    expect_action(ActionKind::None, s.on_join_failed(JoinFail::Other, 10));
    expect_action(ActionKind::None, s.on_disconnected(10));
    expect_state(PlanState::Setup, s);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_sha256_empty);
    RUN_TEST(test_sha256_abc);
    RUN_TEST(test_sha256_448_bits);
    RUN_TEST(test_sha256_million_a);
    RUN_TEST(test_sha256_padding_boundaries_differ);
    RUN_TEST(test_pin_not_set);
    RUN_TEST(test_pin_set_rejects_invalid);
    RUN_TEST(test_pin_ok_and_wrong);
    RUN_TEST(test_pin_hash_is_salted_sha256);
    RUN_TEST(test_pin_four_wrong_then_ok_resets);
    RUN_TEST(test_pin_first_lockout_60s_boundaries);
    RUN_TEST(test_pin_locked_state_does_not_evaluate);
    RUN_TEST(test_pin_second_lockout_120s);
    RUN_TEST(test_pin_lockout_caps_at_3600s);
    RUN_TEST(test_pin_ok_after_lockout_resets_k);
    RUN_TEST(test_pin_lockout_wrap_safe);
    RUN_TEST(test_pin_failures_accumulate_across_callers);
    RUN_TEST(test_pin_record_round_trip);
    RUN_TEST(test_form_plain_field);
    RUN_TEST(test_form_empty_value);
    RUN_TEST(test_form_plus_and_percent);
    RUN_TEST(test_form_utf8);
    RUN_TEST(test_form_key_prefix_not_confused);
    RUN_TEST(test_form_missing_key);
    RUN_TEST(test_form_malformed_escapes);
    RUN_TEST(test_form_body_size_limit);
    RUN_TEST(test_form_out_cap);
    RUN_TEST(test_validate_ssid);
    RUN_TEST(test_validate_password);
    RUN_TEST(test_validate_pin);
    RUN_TEST(test_plan_start_with_saved);
    RUN_TEST(test_plan_start_without_saved);
    RUN_TEST(test_plan_start_twice_ignored);
    RUN_TEST(test_plan_scan_with_candidates_joins_first);
    RUN_TEST(test_plan_scan_without_candidates_goes_to_setup);
    RUN_TEST(test_plan_joined_connects_without_hotspot);
    RUN_TEST(test_plan_join_failure_tries_next_candidate);
    RUN_TEST(test_plan_all_candidates_fail_goes_to_setup);
    RUN_TEST(test_plan_join_timeout_exact);
    RUN_TEST(test_plan_join_timeout_wrap_safe);
    RUN_TEST(test_plan_disconnect_goes_lost_then_setup_at_120s);
    RUN_TEST(test_plan_lost_timer_wrap_safe);
    RUN_TEST(test_plan_lost_reconnect);
    RUN_TEST(test_plan_joined_from_setup_join_stops_hotspot);
    RUN_TEST(test_plan_setup_rescan_every_30s);
    RUN_TEST(test_plan_setup_rescan_wrap_safe);
    RUN_TEST(test_plan_setup_scan_finds_saved_network);
    RUN_TEST(test_plan_setup_empty_scan_changes_nothing);
    RUN_TEST(test_plan_network_added_in_setup_scans);
    RUN_TEST(test_plan_network_added_in_lost_scans);
    RUN_TEST(test_plan_network_added_while_connected_is_none);
    RUN_TEST(test_plan_network_added_in_other_states_is_none);
    RUN_TEST(test_plan_failed_setup_join_keeps_hotspot);
    RUN_TEST(test_plan_failed_setup_join_by_timeout_keeps_hotspot);
    RUN_TEST(test_plan_inapplicable_events_do_nothing);
    return UNITY_END();
}
