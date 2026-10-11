/**
 * @file pin_check.cpp
 * @brief Wiring bring-up check. Exercises each turret part on its own, with no camera or WiFi.
 * @details Build and flash with `pio run -e pin_check -t upload`. Output is on serial at 115200.
 * Sequence: boot report -> stepper quarter turn each way -> servo sweep -> 15 s joystick window -> summary.
 * Press the joystick button afterwards to run it again.
 */

#include <Arduino.h>
#include <ESP32Servo.h>
#include <Stepper.h>
#include <esp_system.h>

#include "pins.h"

static const int STEPS_PER_REV = 2048; // 28BYJ-48 in full-step mode
static const int STEPPER_RPM = 10;
static const uint32_t JOYSTICK_WINDOW_MS = 15000;
static const int AXIS_FULL_SWING = 2500; // of 4095; a working axis easily exceeds this corner to corner

// The 28BYJ-48 coil order needs IN1, IN3, IN2, IN4 for the Arduino Stepper library's sequence.
Stepper stepper(STEPS_PER_REV, pins::STEPPER_IN1, pins::STEPPER_IN3, pins::STEPPER_IN2, pins::STEPPER_IN4);
Servo servo;

static void release_stepper()
{
    // De-energize the coils so the motor and ULN2003 don't heat up while idle
    const uint8_t coils[] = {pins::STEPPER_IN1, pins::STEPPER_IN2, pins::STEPPER_IN3, pins::STEPPER_IN4};
    for (uint8_t pin : coils)
    {
        digitalWrite(pin, LOW);
    }
}

static void check_boot()
{
    Serial.println();
    Serial.println("==== VOID SENTRY PIN CHECK ====");
    Serial.printf("[boot] OK - reached setup() with the stepper on strapping pins 12/15/2\n");
    Serial.printf("[boot] reset reason: %d\n", static_cast<int>(esp_reset_reason()));
    Serial.printf("[boot] PSRAM: %s (%u bytes)\n", psramFound() ? "found" : "MISSING", ESP.getPsramSize());
}

static void check_stepper()
{
    Serial.println("[stepper] quarter turn clockwise...");
    stepper.step(STEPS_PER_REV / 4);
    delay(500);
    Serial.println("[stepper] quarter turn counter-clockwise...");
    stepper.step(-STEPS_PER_REV / 4);
    release_stepper();
    Serial.println("[stepper] done");
}

static void sweep(int from, int to)
{
    int dir = (to > from) ? 1 : -1;
    for (int angle = from; angle != to; angle += dir)
    {
        servo.write(angle);
        delay(12);
    }
    servo.write(to);
}

static void check_servo()
{
    Serial.println("[servo] centre 90 -> 45 -> 135 -> 90");
    servo.write(90);
    delay(600);
    sweep(90, 45);
    sweep(45, 135);
    sweep(135, 90);
    delay(300);
    Serial.println("[servo] done");
}

static void check_joystick()
{
    Serial.printf("[joystick] %lu s window: move the stick to all four corners and press it a few times\n",
                  JOYSTICK_WINDOW_MS / 1000);

    int min_x = 4095, max_x = 0, min_y = 4095, max_y = 0, presses = 0;
    bool was_pressed = false;
    uint32_t start = millis(), last_print = 0;

    while (millis() - start < JOYSTICK_WINDOW_MS)
    {
        int x = analogRead(pins::JOYSTICK_X);
        int y = analogRead(pins::JOYSTICK_Y);
        bool pressed = digitalRead(pins::JOYSTICK_SW) == LOW;

        min_x = min(min_x, x);
        max_x = max(max_x, x);
        min_y = min(min_y, y);
        max_y = max(max_y, y);
        if (pressed && !was_pressed)
        {
            presses++;
        }
        was_pressed = pressed;

        if (millis() - last_print >= 250)
        {
            last_print = millis();
            Serial.printf("[joystick] x=%4d y=%4d button=%s\n", x, y, pressed ? "DOWN" : "up");
        }
        delay(10);
    }

    Serial.println("---- summary ----");
    Serial.printf("[joystick] x range %d..%d -> %s\n", min_x, max_x,
                  (max_x - min_x) > AXIS_FULL_SWING ? "PASS" : "CHECK (no full swing seen)");
    Serial.printf("[joystick] y range %d..%d -> %s\n", min_y, max_y,
                  (max_y - min_y) > AXIS_FULL_SWING ? "PASS" : "CHECK (no full swing seen)");
    Serial.printf("[joystick] button presses: %d -> %s\n", presses, presses > 0 ? "PASS" : "CHECK (none seen)");
}

static void run_all()
{
    check_boot();
    check_stepper();
    check_servo();
    check_joystick();
    Serial.println("==== DONE - press the joystick button to run again ====");
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    pinMode(pins::STEPPER_IN1, OUTPUT);
    pinMode(pins::STEPPER_IN2, OUTPUT);
    pinMode(pins::STEPPER_IN3, OUTPUT);
    pinMode(pins::STEPPER_IN4, OUTPUT);
    release_stepper();
    stepper.setSpeed(STEPPER_RPM);

    servo.setPeriodHertz(50);
    servo.attach(pins::SERVO, 500, 2400);

    pinMode(pins::JOYSTICK_SW, INPUT_PULLUP);
    analogReadResolution(12);

    run_all();
}

void loop()
{
    if (digitalRead(pins::JOYSTICK_SW) == LOW)
    {
        delay(300); // let the press finish before the run starts
        run_all();
    }
    delay(20);
}
