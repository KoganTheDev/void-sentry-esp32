/**
 * @file test_remote.cpp
 * @brief Host-side tests for the remote command parser and the jog/mode inbox.
 */

#include <unity.h>

#include "remote_command.h"
#include "remote_inbox.h"

#include <cstring>

using namespace remote;

void setUp(void) {}
void tearDown(void) {}

static Command parse_str(const char* s) { return parse(s); }

static void expect_move(const char* s, int pan, int tilt)
{
    const Command c = parse_str(s);
    TEST_ASSERT_TRUE_MESSAGE(c.kind == CommandKind::Move, s);
    TEST_ASSERT_EQUAL_INT_MESSAGE(pan, c.pan, s);
    TEST_ASSERT_EQUAL_INT_MESSAGE(tilt, c.tilt, s);
}

static void expect_poll(const char* s) { TEST_ASSERT_TRUE_MESSAGE(parse_str(s).kind == CommandKind::Poll, s); }

// ---- parse ----

void test_parse_moves(void)
{
    expect_move("move:up", 0, 1);
    expect_move("move:down", 0, -1);
    expect_move("move:left", -1, 0);
    expect_move("move:right", 1, 0);
    expect_move("move:up-left", -1, 1);
    expect_move("move:up-right", 1, 1);
    expect_move("move:down-left", -1, -1);
    expect_move("move:down-right", 1, -1);
}

void test_parse_stop_mode_fire(void)
{
    TEST_ASSERT_TRUE(parse_str("move:stop").kind == CommandKind::Stop);
    Command c = parse_str("mode:ai");
    TEST_ASSERT_TRUE(c.kind == CommandKind::Mode);
    TEST_ASSERT_TRUE(c.mode == RemoteMode::Ai);
    c = parse_str("mode:user");
    TEST_ASSERT_TRUE(c.kind == CommandKind::Mode);
    TEST_ASSERT_TRUE(c.mode == RemoteMode::User);
    c = parse_str("fire:on");
    TEST_ASSERT_TRUE(c.kind == CommandKind::Fire);
    TEST_ASSERT_TRUE(c.fire_on);
    c = parse_str("fire:off");
    TEST_ASSERT_TRUE(c.kind == CommandKind::Fire);
    TEST_ASSERT_FALSE(c.fire_on);
}

void test_parse_unknown_is_poll(void)
{
    expect_poll(" ");
    expect_poll("");
    expect_poll("move:");
    expect_poll("move:upp");
    expect_poll("move:up ");
    expect_poll("MOVE:UP");
    expect_poll("mode:calibrate");
    expect_poll("fire:maybe");
    char big[200];
    std::memset(big, 'x', sizeof(big));
    TEST_ASSERT_TRUE(parse({big, sizeof(big)}).kind == CommandKind::Poll);
    TEST_ASSERT_TRUE(parse({}).kind == CommandKind::Poll);
}

void test_parse_reads_only_len_bytes(void)
{
    const Command c = parse({"move:upXYZ", 7});
    TEST_ASSERT_TRUE(c.kind == CommandKind::Move);
    TEST_ASSERT_EQUAL_INT(0, c.pan);
    TEST_ASSERT_EQUAL_INT(1, c.tilt);
    // A prefix of a longer valid word is still exact-matched on len.
    const Command d = parse({"move:up-left", 7});
    TEST_ASSERT_TRUE(d.kind == CommandKind::Move);
    TEST_ASSERT_EQUAL_INT(0, d.pan);
}

void test_parse_cal(void)
{
    Command c = parse_str("cal:start");
    TEST_ASSERT_TRUE(c.kind == CommandKind::Calibrate);
    TEST_ASSERT_TRUE(c.cal == CalAction::Start);
    c = parse_str("cal:point");
    TEST_ASSERT_TRUE(c.kind == CommandKind::Calibrate);
    TEST_ASSERT_TRUE(c.cal == CalAction::Point);
    c = parse_str("cal:cancel");
    TEST_ASSERT_TRUE(c.kind == CommandKind::Calibrate);
    TEST_ASSERT_TRUE(c.cal == CalAction::Cancel);
    expect_poll("cal:");
    expect_poll("cal:stop");
    expect_poll("cal:start ");
    expect_poll("cal:Start");
    expect_poll("cal:points");
    expect_poll("cal");
}

void test_parse_speed(void)
{
    Command c = parse_str("speed:slow");
    TEST_ASSERT_TRUE(c.kind == CommandKind::Speed);
    TEST_ASSERT_TRUE(c.speed == JogSpeed::Slow);
    c = parse_str("speed:normal");
    TEST_ASSERT_TRUE(c.kind == CommandKind::Speed);
    TEST_ASSERT_TRUE(c.speed == JogSpeed::Normal);
    c = parse_str("speed:fast");
    TEST_ASSERT_TRUE(c.kind == CommandKind::Speed);
    TEST_ASSERT_TRUE(c.speed == JogSpeed::Fast);
    expect_poll("speed:");
    expect_poll("speed:medium");
    expect_poll("speed:fast ");
    expect_poll("speed:FAST");
}

static void expect_aim(const char* s, int x, int y)
{
    const Command c = parse_str(s);
    TEST_ASSERT_TRUE_MESSAGE(c.kind == CommandKind::Aim, s);
    TEST_ASSERT_EQUAL_INT_MESSAGE(x, c.aim_x, s);
    TEST_ASSERT_EQUAL_INT_MESSAGE(y, c.aim_y, s);
}

void test_parse_aim_valid(void)
{
    expect_aim("aim:0,0", 0, 0);
    expect_aim("aim:160,120", 160, 120);
    expect_aim("aim:319,239", 319, 239);
    expect_aim("aim:5,7", 5, 7);
    expect_aim("aim:12,99", 12, 99);
    expect_aim("aim:000,000", 0, 0); // plain digits, leading zeros allowed within 3 digits
    expect_aim("aim:007,010", 7, 10);
    expect_aim("aim:319,0", 319, 0);
    expect_aim("aim:0,239", 0, 239);
}

void test_parse_aim_malformed_is_poll(void)
{
    expect_poll("aim:");
    expect_poll("aim:,");
    expect_poll("aim:,5");
    expect_poll("aim:5,");
    expect_poll("aim:5");
    expect_poll("aim:5 5");
    expect_poll("aim:5;5");
    expect_poll("aim:1234,5");
    expect_poll("aim:5,1234");
    expect_poll("aim:0000,0");
    expect_poll("aim:320,0");
    expect_poll("aim:999,0");
    expect_poll("aim:0,240");
    expect_poll("aim:0,999");
    expect_poll("aim:-1,5");
    expect_poll("aim:5,-1");
    expect_poll("aim:+5,5");
    expect_poll("aim:5,+5");
    expect_poll("aim: 5,5");
    expect_poll("aim:5, 5");
    expect_poll("aim:5 ,5");
    expect_poll("aim:5,5 ");
    expect_poll("aim:5,5,");
    expect_poll("aim:5,5,5");
    expect_poll("aim:1,2x");
    expect_poll("aim:x1,2");
    expect_poll("aim:1.5,2");
    expect_poll("aim:1,2.5");
    expect_poll("aim:0x10,5");
    expect_poll("aim:a,b");
    expect_poll("AIM:5,5");
    expect_poll("aim5,5");
    expect_poll("aim,5,5");
}

void test_parse_aim_reads_only_len_bytes(void)
{
    const Command c = parse({"aim:12,34999", 9});
    TEST_ASSERT_TRUE(c.kind == CommandKind::Aim);
    TEST_ASSERT_EQUAL_INT(12, c.aim_x);
    TEST_ASSERT_EQUAL_INT(34, c.aim_y);
    // A shortened view that cuts a number or the comma is malformed, not a different valid number.
    TEST_ASSERT_TRUE(parse({"aim:12,34", 7}).kind == CommandKind::Poll); // "aim:12,"
    TEST_ASSERT_TRUE(parse({"aim:12,34", 6}).kind == CommandKind::Poll); // "aim:12"
    TEST_ASSERT_TRUE(parse({"aim:12,34", 4}).kind == CommandKind::Poll); // "aim:"
    TEST_ASSERT_TRUE(parse({"aim:1,2999", 7}).kind == CommandKind::Aim);
    TEST_ASSERT_TRUE(parse({"cal:startX", 9}).kind == CommandKind::Calibrate);
    TEST_ASSERT_TRUE(parse({"speed:fastX", 10}).kind == CommandKind::Speed);
    TEST_ASSERT_TRUE(parse({"speed:fast", 9}).kind == CommandKind::Poll);
}

// ---- inbox ----

static Command mv(int8_t pan, int8_t tilt)
{
    Command c;
    c.kind = CommandKind::Move;
    c.pan = pan;
    c.tilt = tilt;
    return c;
}

static Command mode_cmd(RemoteMode m)
{
    Command c;
    c.kind = CommandKind::Mode;
    c.mode = m;
    return c;
}

void test_no_jog_before_move(void)
{
    RemoteInbox in;
    TEST_ASSERT_FALSE(in.jog_active(0));
    const Jog j = in.jog(0);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, j.pan);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, j.tilt);
}

void test_jog_after_move_and_timeout(void)
{
    RemoteInbox in;
    in.submit(mv(-1, 1), 1000);
    Jog j = in.jog(1000);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -0.6f, j.pan);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.6f, j.tilt);
    TEST_ASSERT_TRUE(in.jog_active(1299));
    TEST_ASSERT_FALSE(in.jog_active(1300));
    j = in.jog(1300);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, j.pan);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, j.tilt);
}

void test_fresh_move_restarts_window(void)
{
    RemoteInbox in;
    in.submit(mv(1, 0), 1000);
    in.submit(mv(1, 0), 1200);
    TEST_ASSERT_TRUE(in.jog_active(1499));
    TEST_ASSERT_FALSE(in.jog_active(1500));
}

void test_stop_clears_immediately(void)
{
    RemoteInbox in;
    in.submit(mv(0, -1), 1000);
    Command stop;
    stop.kind = CommandKind::Stop;
    in.submit(stop, 1010);
    TEST_ASSERT_FALSE(in.jog_active(1010));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, in.jog(1010).tilt);
}

void test_jog_wraparound(void)
{
    RemoteInbox in;
    in.submit(mv(1, 0), 0xFFFFFF00u);
    TEST_ASSERT_TRUE(in.jog_active(0x00000010u));  // 272 ms later
    TEST_ASSERT_FALSE(in.jog_active(0x00000040u)); // 320 ms later
}

void test_mode_request_taken_once(void)
{
    RemoteInbox in;
    TEST_ASSERT_FALSE(in.take_mode_request().has_value());
    in.submit(mode_cmd(RemoteMode::User), 0);
    auto m = in.take_mode_request();
    TEST_ASSERT_TRUE(m.has_value());
    TEST_ASSERT_TRUE(*m == RemoteMode::User);
    TEST_ASSERT_FALSE(in.take_mode_request().has_value());
}

void test_second_mode_replaces_first(void)
{
    RemoteInbox in;
    in.submit(mode_cmd(RemoteMode::User), 0);
    in.submit(mode_cmd(RemoteMode::Ai), 5);
    auto m = in.take_mode_request();
    TEST_ASSERT_TRUE(m.has_value());
    TEST_ASSERT_TRUE(*m == RemoteMode::Ai);
}

void test_poll_and_fire_change_nothing(void)
{
    RemoteInbox in;
    in.submit(mv(1, 1), 100);
    in.submit(mode_cmd(RemoteMode::User), 100);
    Command fire;
    fire.kind = CommandKind::Fire;
    fire.fire_on = true;
    in.submit(fire, 150);
    in.submit(Command{}, 160);
    TEST_ASSERT_TRUE(in.jog_active(399));
    TEST_ASSERT_FALSE(in.jog_active(400)); // window still anchored at the Move
    auto m = in.take_mode_request();
    TEST_ASSERT_TRUE(m.has_value());
    TEST_ASSERT_TRUE(*m == RemoteMode::User);
}

static Command cal_cmd(CalAction a)
{
    Command c;
    c.kind = CommandKind::Calibrate;
    c.cal = a;
    return c;
}

static Command aim_cmd(int x, int y)
{
    Command c;
    c.kind = CommandKind::Aim;
    c.aim_x = static_cast<int16_t>(x);
    c.aim_y = static_cast<int16_t>(y);
    return c;
}

static Command speed_cmd(JogSpeed s)
{
    Command c;
    c.kind = CommandKind::Speed;
    c.speed = s;
    return c;
}

void test_cal_request_taken_once_latest_wins(void)
{
    RemoteInbox in;
    TEST_ASSERT_FALSE(in.take_cal_request().has_value());
    in.submit(cal_cmd(CalAction::Start), 0);
    auto r = in.take_cal_request();
    TEST_ASSERT_TRUE(r.has_value());
    TEST_ASSERT_TRUE(*r == CalAction::Start);
    TEST_ASSERT_FALSE(in.take_cal_request().has_value());
    in.submit(cal_cmd(CalAction::Point), 1);
    in.submit(cal_cmd(CalAction::Cancel), 2);
    r = in.take_cal_request();
    TEST_ASSERT_TRUE(r.has_value());
    TEST_ASSERT_TRUE(*r == CalAction::Cancel);
    TEST_ASSERT_FALSE(in.take_cal_request().has_value());
}

void test_cal_does_not_touch_jog_or_mode(void)
{
    RemoteInbox in;
    in.submit(mv(1, 0), 100);
    in.submit(cal_cmd(CalAction::Point), 110);
    TEST_ASSERT_TRUE(in.jog_active(120));
    TEST_ASSERT_FALSE(in.take_mode_request().has_value());
    TEST_ASSERT_FALSE(in.take_aim_request().has_value());
}

void test_aim_request_taken_once_latest_wins(void)
{
    RemoteInbox in;
    TEST_ASSERT_FALSE(in.take_aim_request().has_value());
    in.submit(aim_cmd(10, 20), 0);
    in.submit(aim_cmd(300, 200), 5);
    auto a = in.take_aim_request();
    TEST_ASSERT_TRUE(a.has_value());
    TEST_ASSERT_EQUAL_INT(300, a->x);
    TEST_ASSERT_EQUAL_INT(200, a->y);
    TEST_ASSERT_FALSE(in.take_aim_request().has_value());
}

void test_aim_ends_active_jog(void)
{
    RemoteInbox in;
    in.submit(mv(1, 1), 1000);
    TEST_ASSERT_TRUE(in.jog_active(1010));
    in.submit(aim_cmd(1, 2), 1010);
    TEST_ASSERT_FALSE(in.jog_active(1010));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, in.jog(1010).pan);
    // A later Move starts a fresh jog.
    in.submit(mv(-1, 0), 1020);
    TEST_ASSERT_TRUE(in.jog_active(1030));
}

void test_speed_default_and_persistent(void)
{
    RemoteInbox in;
    TEST_ASSERT_TRUE(in.speed() == JogSpeed::Normal);
    in.submit(speed_cmd(JogSpeed::Fast), 0);
    TEST_ASSERT_TRUE(in.speed() == JogSpeed::Fast);
    in.submit(Command{}, 10);
    in.submit(mv(1, 0), 20);
    in.submit(aim_cmd(1, 1), 30);
    TEST_ASSERT_TRUE(in.speed() == JogSpeed::Fast);
    in.submit(speed_cmd(JogSpeed::Slow), 40);
    TEST_ASSERT_TRUE(in.speed() == JogSpeed::Slow);
}

void test_jog_magnitude_follows_speed(void)
{
    RemoteInbox in;
    in.submit(speed_cmd(JogSpeed::Slow), 0);
    in.submit(mv(-1, 1), 100);
    Jog j = in.jog(100);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -0.25f, j.pan);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.25f, j.tilt);
    in.submit(speed_cmd(JogSpeed::Fast), 110);
    j = in.jog(120); // the running jog picks up the new magnitude
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -1.0f, j.pan);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, j.tilt);
    in.submit(speed_cmd(JogSpeed::Normal), 130);
    j = in.jog(140);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -kJogSpeed, j.pan);
    TEST_ASSERT_EQUAL_FLOAT(0.6f, kJogSpeed);
    TEST_ASSERT_EQUAL_FLOAT(0.25f, kJogSpeedSlow);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, kJogSpeedFast);
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_moves);
    RUN_TEST(test_parse_stop_mode_fire);
    RUN_TEST(test_parse_unknown_is_poll);
    RUN_TEST(test_parse_reads_only_len_bytes);
    RUN_TEST(test_parse_cal);
    RUN_TEST(test_parse_speed);
    RUN_TEST(test_parse_aim_valid);
    RUN_TEST(test_parse_aim_malformed_is_poll);
    RUN_TEST(test_parse_aim_reads_only_len_bytes);
    RUN_TEST(test_no_jog_before_move);
    RUN_TEST(test_jog_after_move_and_timeout);
    RUN_TEST(test_fresh_move_restarts_window);
    RUN_TEST(test_stop_clears_immediately);
    RUN_TEST(test_jog_wraparound);
    RUN_TEST(test_mode_request_taken_once);
    RUN_TEST(test_second_mode_replaces_first);
    RUN_TEST(test_poll_and_fire_change_nothing);
    RUN_TEST(test_cal_request_taken_once_latest_wins);
    RUN_TEST(test_cal_does_not_touch_jog_or_mode);
    RUN_TEST(test_aim_request_taken_once_latest_wins);
    RUN_TEST(test_aim_ends_active_jog);
    RUN_TEST(test_speed_default_and_persistent);
    RUN_TEST(test_jog_magnitude_follows_speed);
    return UNITY_END();
}
