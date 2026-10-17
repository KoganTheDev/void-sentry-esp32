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

static Command parse_str(const char* s) { return parse(s, std::strlen(s)); }

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
    TEST_ASSERT_TRUE(parse(big, sizeof(big)).kind == CommandKind::Poll);
    TEST_ASSERT_TRUE(parse(nullptr, 0).kind == CommandKind::Poll);
    TEST_ASSERT_TRUE(parse(nullptr, 5).kind == CommandKind::Poll);
}

void test_parse_reads_only_len_bytes(void)
{
    const Command c = parse("move:upXYZ", 7);
    TEST_ASSERT_TRUE(c.kind == CommandKind::Move);
    TEST_ASSERT_EQUAL_INT(0, c.pan);
    TEST_ASSERT_EQUAL_INT(1, c.tilt);
    // A prefix of a longer valid word is still exact-matched on len.
    const Command d = parse("move:up-left", 7);
    TEST_ASSERT_TRUE(d.kind == CommandKind::Move);
    TEST_ASSERT_EQUAL_INT(0, d.pan);
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

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_moves);
    RUN_TEST(test_parse_stop_mode_fire);
    RUN_TEST(test_parse_unknown_is_poll);
    RUN_TEST(test_parse_reads_only_len_bytes);
    RUN_TEST(test_no_jog_before_move);
    RUN_TEST(test_jog_after_move_and_timeout);
    RUN_TEST(test_fresh_move_restarts_window);
    RUN_TEST(test_stop_clears_immediately);
    RUN_TEST(test_jog_wraparound);
    RUN_TEST(test_mode_request_taken_once);
    RUN_TEST(test_second_mode_replaces_first);
    RUN_TEST(test_poll_and_fire_change_nothing);
    return UNITY_END();
}
