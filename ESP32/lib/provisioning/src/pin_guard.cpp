#include "pin_guard.h"

#include "form.h"
#include "sha256.h"

#include <cstring>

namespace prov
{
namespace
{
std::array<uint8_t, kHashLen> salted_hash(const uint8_t (&salt)[kSaltLen], const char* pin, size_t len)
{
    uint8_t buf[kSaltLen + 8] = {};
    std::memcpy(buf, salt, kSaltLen);
    if (len > 0)
    {
        std::memcpy(buf + kSaltLen, pin, len);
    }
    return sha256(buf, kSaltLen + len);
}
} // namespace

PinGuard::PinGuard(const PinRecord& rec) : _rec(rec) {}

bool PinGuard::set_pin(const char* pin, size_t len, const uint8_t (&salt)[kSaltLen])
{
    if (!valid_pin(pin, len))
    {
        return false;
    }
    PinRecord rec;
    rec.set = true;
    std::memcpy(rec.salt, salt, kSaltLen);
    const auto h = salted_hash(rec.salt, pin, len);
    std::memcpy(rec.hash, h.data(), kHashLen);
    _rec = rec;
    _failures = 0;
    _lockouts = 0;
    _locked = false;
    return true;
}

PinResult PinGuard::check(const char* pin, size_t len, uint32_t now_ms)
{
    PinResult r;
    if (!_rec.set)
    {
        r.status = PinCheck::NotSet;
        return r;
    }
    if (_locked)
    {
        const uint32_t elapsed = static_cast<uint32_t>(now_ms - _lock_start_ms);
        if (elapsed < _lock_len_ms)
        {
            r.status = PinCheck::LockedOut;
            r.seconds_left = (_lock_len_ms - elapsed + 999u) / 1000u;
            return r;
        }
        _locked = false;
    }

    bool match = false;
    if (valid_pin(pin, len))
    {
        const auto h = salted_hash(_rec.salt, pin, len);
        uint8_t diff = 0;
        for (size_t i = 0; i < kHashLen; ++i)
        {
            diff |= static_cast<uint8_t>(h[i] ^ _rec.hash[i]);
        }
        match = (diff == 0);
    }

    if (match)
    {
        _failures = 0;
        _lockouts = 0;
        r.status = PinCheck::Ok;
        return r;
    }

    r.status = PinCheck::Wrong;
    if (++_failures >= kMaxFailures)
    {
        uint32_t len_ms = kBaseLockoutMs;
        for (uint32_t i = 0; i < _lockouts && len_ms < kMaxLockoutMs; ++i)
        {
            len_ms *= 2;
        }
        if (len_ms > kMaxLockoutMs)
        {
            len_ms = kMaxLockoutMs;
        }
        _lock_len_ms = len_ms;
        _lock_start_ms = now_ms;
        _locked = true;
        _failures = 0;
        if (_lockouts < 32)
        {
            ++_lockouts;
        }
    }
    return r;
}

const PinRecord& PinGuard::record() const { return _rec; }
} // namespace prov
