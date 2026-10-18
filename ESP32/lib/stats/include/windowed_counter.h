/**
 * @file windowed_counter.h
 * @brief Per-window deltas of monotonically increasing counters, and the averages/percentages built on them (pure).
 * @details The periodic stats line reports what happened in the last window (about 5 s), not since boot, because
 * since-boot averages hid slowdowns. Each reported quantity is a free-running unsigned counter (frames, bytes,
 * microseconds); WindowedCounter remembers the previous reading and returns the difference. Unsigned
 * subtraction stays correct across a counter wrap, as long as the counter moves less than its range per window.
 */

#pragma once

#include <cstdint>
#include <type_traits>

namespace stats
{
/** @brief Remembers the previous reading of a free-running unsigned counter; sample() returns the delta since. */
template <typename T> class WindowedCounter
{
    static_assert(std::is_unsigned_v<T>, "wrap-safe deltas need unsigned arithmetic");

public:
    /** @brief Record @p total as the new reading and return how much it grew since the previous call (0 first). */
    T sample(T total)
    {
        const T delta = static_cast<T>(total - prev_);
        prev_ = total;
        return delta;
    }

private:
    T prev_ = 0;
};

/** @brief @p sum / @p count, or 0 when @p count is 0 (an empty window). */
template <typename T> constexpr T window_avg(T sum, T count) { return count ? static_cast<T>(sum / count) : T{0}; }

/** @brief 100 * @p part / @p whole as an integer percent, or 0 when @p whole is 0 (an empty window). */
constexpr unsigned window_percent(uint64_t part, uint64_t whole)
{
    return whole ? static_cast<unsigned>(part * 100 / whole) : 0u;
}
} // namespace stats
