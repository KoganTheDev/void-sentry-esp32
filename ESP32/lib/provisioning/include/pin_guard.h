/**
 * @file pin_guard.h
 * @brief PIN storage (salted SHA-256) with constant-time check and escalating lockout (pure C++20, no heap).
 */

#pragma once

#include "prov_types.h"

namespace prov
{
enum class PinCheck { Ok, Wrong, LockedOut, NotSet };

struct PinResult {
    PinCheck status = PinCheck::NotSet;
    uint32_t seconds_left = 0; // LockedOut only, rounded up
};

/**
 * @brief Guards the PIN. The 5th consecutive wrong PIN starts a lockout of min(60 s * 2^k, 3600 s), k = lockouts
 * since the last success; the failure count then resets. Lockout state lives in RAM only.
 */
class PinGuard
{
public:
    static constexpr uint32_t kMaxFailures = 5;
    static constexpr uint32_t kBaseLockoutMs = 60000;
    static constexpr uint32_t kMaxLockoutMs = 3600000;

    explicit PinGuard(const PinRecord& rec = PinRecord{});
    /** @brief Stores hash = sha256(salt || pin). False (nothing changed) if the PIN is not 4-8 digits. */
    bool set_pin(const char* pin, size_t len, const uint8_t (&salt)[kSaltLen]);
    /** @brief Checks a PIN. While locked out nothing is evaluated. Wrap-safe in @p now_ms. */
    PinResult check(const char* pin, size_t len, uint32_t now_ms);
    const PinRecord& record() const;

private:
    PinRecord _rec;
    uint32_t _failures = 0;
    uint32_t _lockouts = 0;
    bool _locked = false;
    uint32_t _lock_start_ms = 0;
    uint32_t _lock_len_ms = 0;
};
} // namespace prov
