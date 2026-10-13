/**
 * @file turret.h
 * @brief Pan stepper + tilt servo driven by a dedicated FreeRTOS motor task.
 */
#pragma once

#include "base_turret.h"
#include "step_planner.h"
#include <ESP32Servo.h>
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/**
 * @class Turret
 * @brief Non-blocking turret: callers post commands, a 1 kHz motor task executes them.
 * @details Why a task: the Arduino stepper library blocks for the whole move (about 15 ms per command), which
 * stalled frame detection in loop(). Here loop() only writes a small command mailbox; the motor task (core 0,
 * priority 3) ticks an aiming::StepPlanner every millisecond, drives the four ULN2003 coil pins itself and moves the
 * servo at 50 Hz with a rate limit.
 * Why the coils are released: a 28BYJ-48 held energized at standstill draws current continuously and heats up, so
 * all four pins go LOW 500 ms after the last step. Its gear train holds the position unpowered.
 * Thread safety: the mailbox and the published state share one portMUX spinlock; critical sections only copy
 * a few words.
 */
class Turret : public BaseTurret
{
public:
    /** @param servo Servo object used for tilt (attached by begin()). */
    explicit Turret(Servo& servo);

    /**
     * @brief Configure pins (coils LOW), attach the servo at home tilt and start the motor task.
     * @return false if the task could not be created. Idempotent: returns true if already running.
     */
    bool begin();

    void set_target_deg(const aiming::Angles& target) override;
    void jog(float pan, float tilt) override;
    void home() override;
    aiming::Angles position_deg() const override;
    float pan_speed() const override;

    /** @brief Count of late wake-ups (lag of more than 1 tick) where missed 1 ms ticks were dropped. */
    uint32_t late_ticks() const;

    /**
     * @brief Minimum free stack of the motor task seen so far, in bytes (high-water mark, sampled every 1000
     * ticks; 0 until the first sample).
     */
    uint32_t stack_free() const;

private:
    enum class CommandKind : uint8_t { None, Target, Jog, Home };

    struct Command {
        CommandKind kind = CommandKind::None;
        aiming::Angles target{0.0f, 0.0f};
        float jog_pan = 0.0f;
        float jog_tilt = 0.0f;
        uint32_t sequence = 0;
    };

    static void task_entry(void* arg);
    void run();
    void write_coils(int phase);
    void release_coils();

    Servo& _servo;
    TaskHandle_t _task = nullptr;
    mutable portMUX_TYPE _lock = portMUX_INITIALIZER_UNLOCKED;
    Command _command;                       ///< guarded by _lock
    aiming::Angles _published{0.0f, 90.0f}; ///< guarded by _lock
    uint32_t _late_ticks = 0;               ///< guarded by _lock
    uint32_t _stack_free = 0;               ///< bytes, guarded by _lock
    float _published_speed = 0.0f;          ///< guarded by _lock

    // Owned by the motor task only.
    aiming::StepPlanner _planner;
    int _phase = 0;
    bool _coils_on = false;
    uint32_t _last_step_ms = 0;
    float _tilt_target = 90.0f;
    float _tilt_current = 90.0f;
    float _tilt_rate = 0.0f;
    int _servo_written = 90;
};
