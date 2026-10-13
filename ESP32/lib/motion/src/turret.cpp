#include "turret.h"
#include "pins.h"
#include <Arduino.h>
#include <cmath>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOSConfig.h>

static const char* TAG = "TURRET";

static_assert(configTICK_RATE_HZ == 1000, "motor task assumes a 1 ms tick");

namespace
{
constexpr float kTiltJogRateDegS = 120.0f; ///< jog tilt rate at full deflection
constexpr float kTiltMaxSlewDegS = 120.0f; ///< servo slew limit
constexpr float kServoPeriodS = 0.02f;     ///< 50 Hz
constexpr uint32_t kServoEveryTicks = 20;  ///< 1 ms ticks per servo update
constexpr uint32_t kCoilReleaseMs = 500;
constexpr uint32_t kStackSampleTicks = 1000; ///< stack high-water mark sample period
constexpr int64_t kMinStepSpacingUs = 1500;  ///< 28BYJ-48 cannot follow coil steps closer than this
constexpr uint8_t kPattern[4][4] = {{1, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 1, 1}, {1, 0, 0, 1}};
constexpr uint8_t kCoilPins[4] = {pins::STEPPER_IN1, pins::STEPPER_IN2, pins::STEPPER_IN3, pins::STEPPER_IN4};

float clamp_tilt(float t)
{
    return t < aiming::kTiltMinDeg ? aiming::kTiltMinDeg : (t > aiming::kTiltMaxDeg ? aiming::kTiltMaxDeg : t);
}

float clamp_unit(float v)
{
    if (!std::isfinite(v))
    {
        return 0.0f;
    }
    return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v);
}
} // namespace

Turret::Turret(Servo& servo) : _servo(servo)
{
    this->_published.tilt = aiming::kTiltHomeDeg;
    this->_tilt_target = aiming::kTiltHomeDeg;
    this->_tilt_current = aiming::kTiltHomeDeg;
    this->_servo_written = static_cast<int>(aiming::kTiltHomeDeg);
}

bool Turret::begin()
{
    if (this->_task != nullptr)
    {
        return true;
    }
    for (uint8_t pin : kCoilPins)
    {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
    this->_coils_on = false;
    this->_servo.attach(pins::SERVO);
    this->_servo.write(this->_servo_written);

    if (xTaskCreatePinnedToCore(task_entry, "motor", 3072, this, 3, &this->_task, 0) != pdPASS)
    {
        this->_task = nullptr;
        ESP_LOGE(TAG, "motor task creation failed");
        return false;
    }
    return true;
}

void Turret::set_target_deg(const aiming::Angles& target)
{
    portENTER_CRITICAL(&this->_lock);
    this->_command.kind = CommandKind::Target;
    this->_command.target = target;
    ++this->_command.sequence;
    portEXIT_CRITICAL(&this->_lock);
}

void Turret::jog(float pan, float tilt)
{
    pan = clamp_unit(pan);
    tilt = clamp_unit(tilt);
    portENTER_CRITICAL(&this->_lock);
    this->_command.kind = CommandKind::Jog;
    this->_command.jog_pan = pan;
    this->_command.jog_tilt = tilt;
    ++this->_command.sequence;
    portEXIT_CRITICAL(&this->_lock);
}

void Turret::home()
{
    portENTER_CRITICAL(&this->_lock);
    this->_command.kind = CommandKind::Home;
    ++this->_command.sequence;
    portEXIT_CRITICAL(&this->_lock);
}

aiming::Angles Turret::position_deg() const
{
    portENTER_CRITICAL(&this->_lock);
    const aiming::Angles a = this->_published;
    portEXIT_CRITICAL(&this->_lock);
    return a;
}

uint32_t Turret::late_ticks() const
{
    portENTER_CRITICAL(&this->_lock);
    const uint32_t n = this->_late_ticks;
    portEXIT_CRITICAL(&this->_lock);
    return n;
}

uint32_t Turret::stack_free() const
{
    portENTER_CRITICAL(&this->_lock);
    const uint32_t n = this->_stack_free;
    portEXIT_CRITICAL(&this->_lock);
    return n;
}

float Turret::pan_speed() const
{
    portENTER_CRITICAL(&this->_lock);
    const float s = this->_published_speed;
    portEXIT_CRITICAL(&this->_lock);
    return s;
}

void Turret::task_entry(void* arg) { static_cast<Turret*>(arg)->run(); }

void Turret::write_coils(int phase)
{
    for (int c = 0; c < 4; ++c)
    {
        digitalWrite(kCoilPins[c], kPattern[phase][c]);
    }
    this->_coils_on = true;
}

void Turret::release_coils()
{
    for (uint8_t pin : kCoilPins)
    {
        digitalWrite(pin, LOW);
    }
    this->_coils_on = false;
}

void Turret::run()
{
    uint32_t handled_seq = 0;
    uint32_t tick_count = 0;
    TickType_t last_wake = xTaskGetTickCount();
    int pending = 0;          // planner steps counted but not yet emitted to the coils
    int64_t last_step_us = 0; // esp_timer time of the last emitted coil step
    uint32_t late = 0;
    uint32_t stack_free = 0; // bytes; sampled every kStackSampleTicks
    uint32_t sample_count = 0;

    for (;;)
    {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1));

        // If the task was starved (WiFi/lwIP on core 0, flash cache-disable windows), FreeRTOS 10.x advances
        // last_wake one tick per call and returns immediately, so missed ticks would run back-to-back within
        // microseconds. Drop them instead: resync to now. The planner just runs slower in wall time.
        const TickType_t now_tick = xTaskGetTickCount();
        const int32_t lag = static_cast<int32_t>(now_tick - last_wake);
        if (lag > 0)
        {
            last_wake = now_tick;
            if (lag > 1) // a lag of exactly 1 tick is ordinary scheduling jitter, not starvation
            {
                ++late;
            }
        }

        // Commands: copy under the lock, apply outside it.
        Command cmd;
        bool fresh = false;
        portENTER_CRITICAL(&this->_lock);
        if (this->_command.sequence != handled_seq)
        {
            cmd = this->_command;
            fresh = true;
        }
        portEXIT_CRITICAL(&this->_lock);

        if (fresh)
        {
            handled_seq = cmd.sequence;
            if (cmd.kind == CommandKind::Target || cmd.kind == CommandKind::Home)
            {
                const aiming::Angles goal = (cmd.kind == CommandKind::Home)
                                                ? aiming::clamp_angles({0.0f, aiming::kTiltHomeDeg})
                                                : aiming::clamp_angles(cmd.target);
                this->_planner.set_target(aiming::pan_deg_to_steps(goal.pan));
                this->_tilt_target = goal.tilt;
                this->_tilt_rate = 0.0f;
            } else if (cmd.kind == CommandKind::Jog)
            {
                this->_planner.set_velocity(cmd.jog_pan * this->_planner.config().max_speed);
                this->_tilt_rate = cmd.jog_tilt * kTiltJogRateDegS;
            }
        }

        // Stepping.
        const uint32_t now_ms = millis();
        // The planner's position already counts every step it returns. Steps are queued in `pending` and emitted at
        // most one per kMinStepSpacingUs, so two coil steps are never closer than the motor can follow (a p -> p+2
        // jump would stall it and corrupt the open-loop step count). Held steps are never dropped: the coil phase
        // catches up exactly once per planner step.
        pending += this->_planner.tick();
        const int64_t now_us = esp_timer_get_time();
        if (pending != 0 && (now_us - last_step_us) >= kMinStepSpacingUs)
        {
            const int step = pending > 0 ? 1 : -1;
            pending -= step;
            if (!this->_coils_on)
            {
                this->write_coils(this->_phase); // energize on the current phase before moving
            }
            this->_phase = (this->_phase + step) & 3;
            this->write_coils(this->_phase);
            this->_last_step_ms = now_ms;
            last_step_us = now_us;
        } else if (pending == 0 && this->_coils_on && this->_planner.is_idle() &&
                   (now_ms - this->_last_step_ms) > kCoilReleaseMs)
        {
            this->release_coils();
        }

        // Servo at 50 Hz.
        if (++tick_count % kServoEveryTicks == 0)
        {
            if (this->_tilt_rate != 0.0f)
            {
                this->_tilt_target = clamp_tilt(this->_tilt_target + this->_tilt_rate * kServoPeriodS);
            }
            const float max_delta = kTiltMaxSlewDegS * kServoPeriodS;
            const float diff = this->_tilt_target - this->_tilt_current;
            this->_tilt_current += (diff > max_delta) ? max_delta : (diff < -max_delta ? -max_delta : diff);
            const int rounded = static_cast<int>(std::lround(this->_tilt_current));
            if (rounded != this->_servo_written)
            {
                this->_servo.write(rounded);
                this->_servo_written = rounded;
            }
        }

        // Stack high-water mark, sampled rarely (it walks the stack). Computed outside the lock.
        if (++sample_count >= kStackSampleTicks)
        {
            sample_count = 0;
            stack_free = static_cast<uint32_t>(uxTaskGetStackHighWaterMark(nullptr)) * sizeof(StackType_t);
        }

        // Publish.
        const float pan = aiming::pan_steps_to_deg(this->_planner.position());
        const float tilt = this->_tilt_current;
        const float speed = this->_planner.speed();
        portENTER_CRITICAL(&this->_lock);
        this->_published.pan = pan;
        this->_published.tilt = tilt;
        this->_published_speed = speed;
        this->_late_ticks = late;
        this->_stack_free = stack_free;
        portEXIT_CRITICAL(&this->_lock);
    }
}
