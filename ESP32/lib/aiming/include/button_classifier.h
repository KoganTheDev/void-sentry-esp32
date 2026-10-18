/**
 * @file button_classifier.h
 * @brief Turns a raw push-button level into Short / Long press events (pure C++20, natively tested).
 * @details Debounces the raw level (it must be stable for debounce_ms). A press released before long_ms after it was
 * confirmed is Short (reported on release). A press still held long_ms after confirmation is Long (reported once,
 * at that moment; the release then reports nothing). Call update() every loop pass with millis().
 */

#pragma once

#include <cstdint>

namespace aiming
{
enum class ButtonEvent : uint8_t { None, Short, Long };

struct ButtonConfig {
    uint32_t debounce_ms = 50;
    uint32_t long_ms = 1500;
};

class ButtonClassifier
{
public:
    explicit ButtonClassifier(const ButtonConfig& config = ButtonConfig{}) : config_(config) {}
    ButtonEvent update(bool pressed_raw, uint32_t now_ms);

private:
    ButtonConfig config_;
    bool raw_ = false;
    uint32_t raw_since_ms_ = 0;
    bool stable_ = false;
    uint32_t pressed_at_ms_ = 0;
    bool long_fired_ = false;
};
} // namespace aiming
