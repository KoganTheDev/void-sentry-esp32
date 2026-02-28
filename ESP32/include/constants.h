#pragma once

#define BAUDRATE 115200

// --- Joystick Config ---
// NOTE: Using ADC pins that don't conflict with camera data lines
#define JOYSTICK_PIN_X 32 // ADC - safe
#define JOYSTICK_PIN_Y 33 // ADC - safe
#define JOYSTICK_PIN_Z 14 // GPIO14 - safe digital pin

// --- Servo Config ---
#define SERVO_PIN 15

// --- Stepper Config ---
// NOTE: Using GPIO pins that don't conflict with camera bus (Y2-Y9, XCLK, PCLK, VSYNC, HREF, SIOD, SIOC)
// Safe available pins: 0, 1, 3, 8, 9, 10, 11, 14, 16, 17 (avoid 2, 12 - strapping pins)
#define STEPPER_NUMBER_OF_STEPS 2048
#define STEPPER_PIN1 13 // OK
#define STEPPER_PIN2 33 // OK
#define STEPPER_PIN3 16 // Safe GPIO16
#define STEPPER_PIN4 17 // Safe GPIO17