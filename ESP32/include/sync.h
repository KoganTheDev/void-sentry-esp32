/**
 * @file sync.h
 * @brief Small RAII wrappers for the two locking patterns the firmware uses (device only).
 * @details MutexLock is for FreeRTOS mutexes held across longer work (never in an ISR). SpinGuarded is for short
 * copy-only critical sections on a value shared between tasks (portENTER_CRITICAL, so no blocking calls, no logging
 * and no allocation inside the callbacks). Both are task-context only: nothing here is ISR-safe, and no current user
 * calls them from an ISR.
 */

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/semphr.h>

namespace vsync
{
/**
 * @brief Scoped lock on a FreeRTOS mutex; takes it forever on construction and gives it back on destruction.
 * @details A null handle is tolerated and makes the lock a no-op, so a manager whose mutex creation failed (and
 * which already reports that) does not crash on every call afterwards.
 */
class MutexLock
{
public:
    explicit MutexLock(SemaphoreHandle_t mutex) : _mutex(mutex)
    {
        if (this->_mutex != nullptr)
        {
            xSemaphoreTake(this->_mutex, portMAX_DELAY);
        }
    }
    ~MutexLock()
    {
        if (this->_mutex != nullptr)
        {
            xSemaphoreGive(this->_mutex);
        }
    }
    MutexLock(const MutexLock&) = delete;
    MutexLock& operator=(const MutexLock&) = delete;

private:
    SemaphoreHandle_t _mutex;
};

/**
 * @brief A value of type T that owns its own spinlock; every access is one short critical section.
 * @details T should be a small trivially copyable struct. load() and store() copy the whole value, update() and
 * inspect() run a callback on the live value under the lock, so a callback must only copy or assign (no blocking
 * calls, no logging, no allocation) to keep the section as short as the hand-written enter/exit pairs it replaces.
 */
template <typename T> class SpinGuarded
{
public:
    SpinGuarded() = default;
    explicit SpinGuarded(const T& initial) : _value(initial) {}
    SpinGuarded(const SpinGuarded&) = delete;
    SpinGuarded& operator=(const SpinGuarded&) = delete;

    /** @brief Returns a copy of the value, made under the lock. */
    T load() const
    {
        const Section section(&this->_lock);
        return this->_value;
    }

    /** @brief Replaces the value under the lock. */
    void store(const T& value)
    {
        const Section section(&this->_lock);
        this->_value = value;
    }

    /** @brief Runs @p f(T&) under the lock and returns its result (void is fine). */
    template <typename F> decltype(auto) update(F&& f)
    {
        const Section section(&this->_lock);
        return f(this->_value);
    }

    /** @brief Runs @p f(const T&) under the lock and returns its result; use it to copy out one field. */
    template <typename F> decltype(auto) inspect(F&& f) const
    {
        const Section section(&this->_lock);
        return f(this->_value);
    }

private:
    /** @brief Enters the critical section on construction and leaves it after the return value was copied out. */
    class Section
    {
    public:
        explicit Section(portMUX_TYPE* lock) : _lock(lock) { portENTER_CRITICAL(this->_lock); }
        ~Section() { portEXIT_CRITICAL(this->_lock); }
        Section(const Section&) = delete;
        Section& operator=(const Section&) = delete;

    private:
        portMUX_TYPE* _lock;
    };

    mutable portMUX_TYPE _lock = portMUX_INITIALIZER_UNLOCKED;
    T _value{};
};
} // namespace vsync
