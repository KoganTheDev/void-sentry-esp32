/**
 * @file test_reply_json.cpp
 * @brief Host-side tests for the /commands JSON reply formatter.
 */

#include <unity.h>

#include "reply_json.h"

#include <cfloat>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

using namespace hud;

void setUp(void) {}
void tearDown(void) {}

namespace
{
/** @brief The reply builder of the commands_handler version before the HUD overhaul, kept as the compatibility oracle.
 */
std::string legacy_reply(const TrackSnapshot* tp, const ControlStatus* sp)
{
    char out[1024];
    const size_t size = sizeof(out);
    const int cap = static_cast<int>(size);
    if (tp == nullptr)
    {
        snprintf(out, size, "{\"detected\":0,\"x\":0,\"y\":0,\"width\":320,\"height\":240,\"pixels\":0}");
        return out;
    }
    const TrackSnapshot& t = *tp;
    int len = snprintf(out, size,
                       "{\"detected\":%d,\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d,\"pixels\":%d,"
                       "\"state\":\"%s\",\"id\":%u,\"bx\":%d,\"by\":%d,\"bw\":%d,\"bh\":%d,"
                       "\"vx\":%.1f,\"vy\":%.1f",
                       t.detected() ? 1 : 0, t.x, t.y, t.frame_width, t.frame_height, t.area_px,
                       tracking::to_string(t.state), static_cast<unsigned>(t.id), t.bx, t.by, t.bw, t.bh,
                       static_cast<double>(t.vx), static_cast<double>(t.vy));
    if (len > 0 && len < cap && sp != nullptr)
    {
        const ControlStatus& c = *sp;
        const int extra = snprintf(out + len, size - len,
                                   ",\"mode\":\"%s\",\"cal\":%u,\"cal_state\":\"%s\",\"calibrated\":%d,"
                                   "\"pan\":%.1f,\"tilt\":%.1f,\"cal_error\":\"%s\"",
                                   to_string(c.mode), static_cast<unsigned>(c.cal_points), cal_state_name(c.cal_state),
                                   c.calibrated ? 1 : 0, static_cast<double>(c.angles.pan),
                                   static_cast<double>(c.angles.tilt), c.cal_failure ? c.cal_failure : "");
        len = (extra < 0) ? -1 : len + extra;
    }
    out[len++] = '}';
    out[len] = '\0';
    return out;
}

TrackSnapshot typical_track()
{
    TrackSnapshot t;
    t.state = tracking::TrackPhase::Locked;
    t.id = 7;
    t.x = 160;
    t.y = 120;
    t.bx = 150;
    t.by = 110;
    t.bw = 20;
    t.bh = 20;
    t.vx = 1.5f;
    t.vy = -0.5f;
    t.area_px = 400;
    return t;
}

ControlStatus typical_status()
{
    ControlStatus c;
    c.mode = ControlMode::User;
    c.cal_points = 1;
    c.cal_state = aiming::CalState::WaitPoint2;
    c.calibrated = true;
    c.angles = aiming::Angles{12.34f, 95.0f};
    c.cal_saves = 3;
    c.cal_miss = 2;
    c.stick = true;
    c.aim_x = 161;
    c.aim_y = -4;
    c.lim = 5;
    c.rx0 = 0;
    c.ry0 = 10;
    c.rx1 = 319;
    c.ry1 = 200;
    c.home_ms = 1400;
    c.returning = true;
    c.speed = 2;
    return c;
}

/** @brief The only calibration error text the firmware produces, taken from the session itself. */
const char* real_cal_error()
{
    static aiming::CalibrationSession session;
    session.start();
    session.record(10.0f, 10.0f, aiming::Angles{0.0f, 90.0f}, true);
    session.record(11.0f, 11.0f, aiming::Angles{1.0f, 91.0f}, true);
    return session.failure();
}

/** @brief Every field at the widest value its format can print. */
void worst_case(TrackSnapshot& t, ControlStatus& c)
{
    t.state = tracking::TrackPhase::Coasting;
    t.id = UINT32_MAX;
    t.x = t.y = t.bx = t.by = t.bw = t.bh = INT_MIN;
    t.frame_width = t.frame_height = t.area_px = INT_MIN;
    t.vx = t.vy = -FLT_MAX;
    c.mode = ControlMode::Calibrate;
    c.cal_points = UINT8_MAX;
    c.cal_state = aiming::CalState::WaitPoint1;
    c.calibrated = true;
    c.angles = aiming::Angles{-FLT_MAX, -FLT_MAX};
    c.cal_failure = real_cal_error();
    c.cal_saves = c.cal_miss = c.home_ms = UINT32_MAX;
    c.stick = c.returning = true;
    c.aim_x = c.aim_y = c.rx0 = c.ry0 = c.rx1 = c.ry1 = INT16_MIN;
    c.lim = UINT8_MAX;
    c.speed = 1; // "normal" is the longest speed name
}

/** @brief An out-of-range access level prints as 3 digits, the widest "auth" value. */
constexpr AuthLevel kWidestAuth = static_cast<AuthLevel>(UINT8_MAX);
} // namespace

void test_golden_typical_reply(void)
{
    const TrackSnapshot t = typical_track();
    const ControlStatus c = typical_status();
    char out[kReplySize];
    const size_t n = format_reply({&t, &c, 2, AuthLevel::Authed}, out, sizeof(out));
    const char* expected =
        "{\"detected\":1,\"x\":160,\"y\":120,\"width\":320,\"height\":240,\"pixels\":400,\"state\":\"locked\","
        "\"id\":7,\"bx\":150,\"by\":110,\"bw\":20,\"bh\":20,\"vx\":1.5,\"vy\":-0.5,"
        "\"mode\":\"user\",\"cal\":1,\"cal_state\":\"point2\",\"calibrated\":1,\"pan\":12.3,\"tilt\":95.0,"
        "\"cal_error\":\"\",\"cal_saves\":3,\"cal_miss\":2,\"stick\":1,\"ax\":161,\"ay\":-4,\"lim\":5,"
        "\"rx0\":0,\"ry0\":10,\"rx1\":319,\"ry1\":200,\"home\":1400,\"ret\":1,\"spd\":\"fast\",\"vw\":2,"
        "\"auth\":1}";
    TEST_ASSERT_EQUAL_STRING(expected, out);
    TEST_ASSERT_EQUAL_UINT32(strlen(expected), n);
}

void test_cal_error_text_is_sent(void)
{
    const TrackSnapshot t = typical_track();
    ControlStatus c = typical_status();
    c.cal_state = aiming::CalState::Failed;
    c.cal_failure = "points too close";
    char out[kReplySize];
    format_reply({&t, &c, 0}, out, sizeof(out));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"cal_state\":\"failed\",\"calibrated\":1,\"pan\":12.3,\"tilt\":95.0,"
                                     "\"cal_error\":\"points too close\",\"cal_saves\":3"));
}

void test_old_keys_are_byte_identical_to_the_legacy_builder(void)
{
    TrackSnapshot t = typical_track();
    ControlStatus c = typical_status();
    c.cal_failure = "points too close";
    const tracking::TrackPhase phases[] = {tracking::TrackPhase::Idle, tracking::TrackPhase::Tentative,
                                           tracking::TrackPhase::Locked, tracking::TrackPhase::Coasting};
    const ControlMode modes[] = {ControlMode::Ai, ControlMode::User, ControlMode::Calibrate};
    const aiming::CalState states[] = {aiming::CalState::Idle, aiming::CalState::WaitPoint1,
                                       aiming::CalState::WaitPoint2, aiming::CalState::Done, aiming::CalState::Failed};
    for (tracking::TrackPhase ph : phases)
    {
        for (ControlMode m : modes)
        {
            for (aiming::CalState st : states)
            {
                t.state = ph;
                t.vx = (ph == tracking::TrackPhase::Idle) ? 0.0f : -3.25f;
                c.mode = m;
                c.cal_state = st;
                char out[kReplySize];
                // Tracker only: identical to the old reply including the closing brace.
                format_reply({&t, nullptr, 0}, out, sizeof(out));
                TEST_ASSERT_EQUAL_STRING(legacy_reply(&t, nullptr).c_str(), out);
                // With a status: the old reply minus its closing brace is a prefix of the new one.
                format_reply({&t, &c, 1}, out, sizeof(out));
                const std::string old = legacy_reply(&t, &c);
                const std::string prefix = old.substr(0, old.size() - 1);
                TEST_ASSERT_EQUAL_INT(0, strncmp(out, prefix.c_str(), prefix.size()));
                TEST_ASSERT_EQUAL_CHAR(',', out[prefix.size()]);
            }
        }
    }
    // Negative coordinates and a missing failure text.
    t.x = -5;
    t.bx = -10;
    c.cal_failure = nullptr;
    char out[kReplySize];
    format_reply({&t, nullptr, 0}, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING(legacy_reply(&t, nullptr).c_str(), out);
}

void test_null_track_sends_the_fixed_fallback(void)
{
    const ControlStatus c = typical_status();
    char out[kReplySize];
    const char* expected = "{\"detected\":0,\"x\":0,\"y\":0,\"width\":320,\"height\":240,\"pixels\":0}";
    TEST_ASSERT_EQUAL_UINT32(strlen(expected), format_reply({nullptr, nullptr, 0}, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(expected, out);
    TEST_ASSERT_EQUAL_STRING(legacy_reply(nullptr, nullptr).c_str(), out);
    // The status is not appended to the fallback.
    format_reply({nullptr, &c, 2}, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING(expected, out);
}

void test_null_status_omits_controller_keys_and_vw(void)
{
    const TrackSnapshot t = typical_track();
    char out[kReplySize];
    const size_t n = format_reply({&t, nullptr, 2}, out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT32(strlen(out), n);
    TEST_ASSERT_NULL(strstr(out, "\"mode\""));
    TEST_ASSERT_NULL(strstr(out, "\"vw\""));
    TEST_ASSERT_NULL(strstr(out, "\"auth\""));
    TEST_ASSERT_EQUAL_CHAR('}', out[n - 1]);
    TEST_ASSERT_EQUAL_STRING("\"vy\":-0.5}", out + n - 10);
}

void test_worst_case_fits_in_reply_size(void)
{
    TrackSnapshot t;
    ControlStatus c;
    worst_case(t, c);
    char out[4096];
    const size_t n = format_reply({&t, &c, UINT32_MAX, kWidestAuth}, out, sizeof(out));
    TEST_ASSERT_EQUAL_UINT32(strlen(out), n);
    printf("worst-case reply length: %u of kReplySize %u\n", static_cast<unsigned>(n),
           static_cast<unsigned>(kReplySize));
    TEST_ASSERT_TRUE_MESSAGE(n < kReplySize, "worst-case reply does not fit in kReplySize");
    // It really is the full reply, not the overflow fallback, and the real size buffer holds it.
    TEST_ASSERT_EQUAL_CHAR('}', out[n - 1]);
    TEST_ASSERT_NOT_NULL(strstr(out, "\"vw\":4294967295,\"auth\":255}"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"cal_error\":\"points too close"));
    char exact[kReplySize];
    TEST_ASSERT_EQUAL_UINT32(n, format_reply({&t, &c, UINT32_MAX, kWidestAuth}, exact, sizeof(exact)));
    TEST_ASSERT_EQUAL_STRING(out, exact);
    // Margin so a small future key or a longer error text does not silently fall back.
    TEST_ASSERT_TRUE_MESSAGE(n + 64 <= kReplySize, "kReplySize has less than 64 bytes of slack");
}

void test_small_buffers_fall_back(void)
{
    const TrackSnapshot t = typical_track();
    const ControlStatus c = typical_status();
    char full[kReplySize];
    const size_t n = format_reply({&t, &c, 1}, full, sizeof(full));
    char out[kReplySize];
    // Exactly the length + brace + NUL fits; one byte less does not.
    TEST_ASSERT_EQUAL_UINT32(n, format_reply({&t, &c, 1}, out, n + 1));
    TEST_ASSERT_EQUAL_STRING(full, out);
    TEST_ASSERT_EQUAL_UINT32(14, format_reply({&t, &c, 1}, out, n));
    TEST_ASSERT_EQUAL_STRING("{\"detected\":0}", out);
    // Too small for the status part, for the tracker part, and barely big enough for the fallback itself.
    TEST_ASSERT_EQUAL_UINT32(14, format_reply({&t, &c, 1}, out, 200));
    TEST_ASSERT_EQUAL_STRING("{\"detected\":0}", out);
    TEST_ASSERT_EQUAL_UINT32(14, format_reply({&t, &c, 1}, out, 40));
    TEST_ASSERT_EQUAL_STRING("{\"detected\":0}", out);
    TEST_ASSERT_EQUAL_UINT32(14, format_reply({&t, &c, 1}, out, 15));
    TEST_ASSERT_EQUAL_STRING("{\"detected\":0}", out);
    // Without a status the tracker-only reply also falls back when it does not fit.
    TEST_ASSERT_EQUAL_UINT32(14, format_reply({&t, nullptr, 0}, out, 100));
    TEST_ASSERT_EQUAL_STRING("{\"detected\":0}", out);
    // Every size from 1 to a full reply: never writes past the buffer, always NUL-terminated within it.
    for (size_t size = 1; size <= n + 2; ++size)
    {
        char guarded[kReplySize + 1];
        std::memset(guarded, 'Z', sizeof(guarded));
        const size_t len = format_reply({&t, &c, 1}, guarded, size);
        TEST_ASSERT_TRUE(len < size);
        TEST_ASSERT_EQUAL_CHAR('\0', guarded[len]);
        TEST_ASSERT_EQUAL_CHAR('Z', guarded[size]);
    }
}

void test_buffers_too_small_even_for_the_fallback(void)
{
    const TrackSnapshot t = typical_track();
    char out[32];
    std::memset(out, 'Z', sizeof(out));
    TEST_ASSERT_EQUAL_UINT32(0, format_reply({&t, nullptr, 0}, out, 14)); // needs 15 bytes with the NUL
    TEST_ASSERT_EQUAL_CHAR('\0', out[0]);
    TEST_ASSERT_EQUAL_CHAR('Z', out[14]);
    TEST_ASSERT_EQUAL_UINT32(0, format_reply({nullptr, nullptr, 0}, out, 20)); // the no-detection text is longer
    TEST_ASSERT_EQUAL_CHAR('\0', out[0]);
    // A zero-sized buffer is never touched.
    std::memset(out, 'Z', sizeof(out));
    TEST_ASSERT_EQUAL_UINT32(0, format_reply({&t, nullptr, 0}, out, 0));
    TEST_ASSERT_EQUAL_UINT32(0, format_reply({nullptr, nullptr, 0}, out, 0));
    TEST_ASSERT_EQUAL_CHAR('Z', out[0]);
}

void test_names(void)
{
    TEST_ASSERT_EQUAL_STRING("idle", cal_state_name(aiming::CalState::Idle));
    TEST_ASSERT_EQUAL_STRING("point1", cal_state_name(aiming::CalState::WaitPoint1));
    TEST_ASSERT_EQUAL_STRING("point2", cal_state_name(aiming::CalState::WaitPoint2));
    TEST_ASSERT_EQUAL_STRING("done", cal_state_name(aiming::CalState::Done));
    TEST_ASSERT_EQUAL_STRING("failed", cal_state_name(aiming::CalState::Failed));
    TEST_ASSERT_EQUAL_STRING("idle", cal_state_name(static_cast<aiming::CalState>(99)));
    TEST_ASSERT_EQUAL_STRING("slow", speed_name(0));
    TEST_ASSERT_EQUAL_STRING("normal", speed_name(1));
    TEST_ASSERT_EQUAL_STRING("fast", speed_name(2));
    TEST_ASSERT_EQUAL_STRING("normal", speed_name(200));
}

void test_each_speed_and_flag_is_formatted(void)
{
    const TrackSnapshot t = typical_track();
    ControlStatus c;
    char out[kReplySize];
    format_reply({&t, &c, 0}, out, sizeof(out));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"mode\":\"ai\",\"cal\":0,\"cal_state\":\"idle\",\"calibrated\":0,"));
    TEST_ASSERT_NOT_NULL(
        strstr(out, "\"stick\":0,\"ax\":0,\"ay\":0,\"lim\":0,\"rx0\":0,\"ry0\":0,\"rx1\":0,\"ry1\":0,"));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"home\":0,\"ret\":0,\"spd\":\"normal\",\"vw\":0,\"auth\":0}"));
    c.speed = 0;
    format_reply({&t, &c, 1}, out, sizeof(out));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"spd\":\"slow\",\"vw\":1,\"auth\":0}"));
}

void test_each_auth_level_is_the_last_key(void)
{
    const TrackSnapshot t = typical_track();
    const ControlStatus c = typical_status();
    char out[kReplySize];
    const struct {
        AuthLevel level;
        const char* tail;
    } cases[] = {{AuthLevel::Viewer, "\"vw\":1,\"auth\":0}"},
                 {AuthLevel::Authed, "\"vw\":1,\"auth\":1}"},
                 {AuthLevel::Open, "\"vw\":1,\"auth\":2}"}};
    for (const auto& k : cases)
    {
        const size_t n = format_reply({&t, &c, 1, k.level}, out, sizeof(out));
        const size_t tail = strlen(k.tail);
        TEST_ASSERT_TRUE(n > tail);
        TEST_ASSERT_EQUAL_STRING(k.tail, out + n - tail);
    }
    // The default input is a viewer: a caller that forgets the level never reports more access than it has.
    format_reply({&t, &c, 1}, out, sizeof(out));
    TEST_ASSERT_NOT_NULL(strstr(out, "\"auth\":0}"));
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_golden_typical_reply);
    RUN_TEST(test_cal_error_text_is_sent);
    RUN_TEST(test_old_keys_are_byte_identical_to_the_legacy_builder);
    RUN_TEST(test_null_track_sends_the_fixed_fallback);
    RUN_TEST(test_null_status_omits_controller_keys_and_vw);
    RUN_TEST(test_worst_case_fits_in_reply_size);
    RUN_TEST(test_small_buffers_fall_back);
    RUN_TEST(test_buffers_too_small_even_for_the_fallback);
    RUN_TEST(test_names);
    RUN_TEST(test_each_speed_and_flag_is_formatted);
    RUN_TEST(test_each_auth_level_is_the_last_key);
    return UNITY_END();
}
