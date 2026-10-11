/**
 * @file pins.h
 * @brief Single source of truth for every GPIO the turret uses.
 * @details Board: Freenove ESP32-WROVER CAM. With the camera attached only GPIO 0, 2, 12, 13, 14, 15, 32 and 33
 * are free, and the turret needs all eight. The static_asserts below turn a bad assignment into a build error
 * instead of a board that corrupts PSRAM or refuses to boot.
 */

#pragma once

#include <stdint.h>

namespace pins
{
// --- Joystick (analog axes must be on ADC1: ADC2 stops working while WiFi is on) ---
constexpr uint8_t JOYSTICK_X = 32;
constexpr uint8_t JOYSTICK_Y = 33;
constexpr uint8_t JOYSTICK_SW = 0; // Shared with the BOOT button; pulled up, pressed = LOW

// --- Servo (pitch) ---
constexpr uint8_t SERVO = 14;

// --- Stepper (yaw), 28BYJ-48 on a ULN2003 board ---
// 12, 15 and 2 are strapping pins. That is safe only because ULN2003 inputs pull toward ground, which is the level
// those pins need at boot. Never move the stepper onto GPIO 0, which must be high at boot.
constexpr uint8_t STEPPER_IN1 = 13;
constexpr uint8_t STEPPER_IN2 = 12;
constexpr uint8_t STEPPER_IN3 = 15;
constexpr uint8_t STEPPER_IN4 = 2;

// --- Camera bus (fixed by the Freenove WROVER board) ---
constexpr int8_t CAM_PWDN = -1;  // Not wired: sensor is always powered
constexpr int8_t CAM_RESET = -1; // Not wired: reset by an RC circuit
constexpr uint8_t CAM_XCLK = 21;
constexpr uint8_t CAM_SIOD = 26; // SCCB (I2C) data
constexpr uint8_t CAM_SIOC = 27; // SCCB (I2C) clock
constexpr uint8_t CAM_Y9 = 35;
constexpr uint8_t CAM_Y8 = 34;
constexpr uint8_t CAM_Y7 = 39;
constexpr uint8_t CAM_Y6 = 36;
constexpr uint8_t CAM_Y5 = 19;
constexpr uint8_t CAM_Y4 = 18;
constexpr uint8_t CAM_Y3 = 5;
constexpr uint8_t CAM_Y2 = 4;
constexpr uint8_t CAM_VSYNC = 25;
constexpr uint8_t CAM_HREF = 23;
constexpr uint8_t CAM_PCLK = 22;

constexpr uint8_t CAMERA[] = {CAM_XCLK, CAM_SIOD, CAM_SIOC, CAM_Y9, CAM_Y8,    CAM_Y7,   CAM_Y6,
                              CAM_Y5,   CAM_Y4,   CAM_Y3,   CAM_Y2, CAM_VSYNC, CAM_HREF, CAM_PCLK};

constexpr uint8_t TURRET[] = {JOYSTICK_X,  JOYSTICK_Y,  JOYSTICK_SW, SERVO,
                              STEPPER_IN1, STEPPER_IN2, STEPPER_IN3, STEPPER_IN4};

namespace check
{
constexpr unsigned TURRET_COUNT = sizeof(TURRET) / sizeof(TURRET[0]);
constexpr unsigned CAMERA_COUNT = sizeof(CAMERA) / sizeof(CAMERA[0]);

// GPIO 6-11 drive the internal flash; 16/17 drive the WROVER's PSRAM, which holds the camera frame buffers.
constexpr bool is_reserved(uint8_t p) { return (p >= 6 && p <= 11) || p == 16 || p == 17; }

constexpr bool any_reserved(unsigned i = 0)
{
    return i < TURRET_COUNT && (is_reserved(TURRET[i]) || any_reserved(i + 1));
}

constexpr bool on_camera(uint8_t p, unsigned i = 0)
{
    return i < CAMERA_COUNT && (CAMERA[i] == p || on_camera(p, i + 1));
}

constexpr bool any_on_camera(unsigned i = 0)
{
    return i < TURRET_COUNT && (on_camera(TURRET[i]) || any_on_camera(i + 1));
}

constexpr bool any_duplicate(unsigned i = 0, unsigned j = 1)
{
    return i < TURRET_COUNT &&
           (j < TURRET_COUNT ? (TURRET[i] == TURRET[j] || any_duplicate(i, j + 1)) : any_duplicate(i + 1, i + 2));
}

constexpr bool is_adc1(uint8_t p) { return p >= 32 && p <= 39; }
} // namespace check

static_assert(!check::any_reserved(), "A turret pin uses GPIO 6-11 (flash) or 16/17 (PSRAM)");
static_assert(!check::any_on_camera(), "A turret pin collides with the camera bus");
static_assert(!check::any_duplicate(), "Two turret signals share a GPIO");
static_assert(check::is_adc1(JOYSTICK_X) && check::is_adc1(JOYSTICK_Y), "Joystick axes must be on ADC1 (GPIO 32-39)");
static_assert(STEPPER_IN1 != 0 && STEPPER_IN2 != 0 && STEPPER_IN3 != 0 && STEPPER_IN4 != 0,
              "The ULN2003 would hold GPIO 0 low at boot and force flash mode");
} // namespace pins
