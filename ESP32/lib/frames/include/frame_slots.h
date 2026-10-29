/**
 * @file frame_slots.h
 * @brief Slot bookkeeping for camera frames: who may write which slot, and RAII leases for readers.
 * @details Pure C++20, no platform headers, so it is unit tested natively (with ASan/UBSan in CI).
 * NOT thread-safe on its own: on the device, FrameStore serializes every call with a mutex.
 *
 * A slot is Free, Writing or Published. One Published slot is the latest. A Published slot that is no longer
 * the latest becomes Free when its last reader releases it, so a slot with readers is never rewritten.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace frames
{
// One writing + one latest + two readers holding older frames. Up to three readers exist (two queued /stream sends
// plus the detect task); if all three hold frames older than the latest, begin_write() finds no Free slot and the
// new camera frame is dropped and counted in dropped_no_slot rather than rewriting a leased slot.
constexpr size_t kSlotCount = 4;

struct FrameMeta {
    size_t length = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    uint32_t timestamp_ms = 0;
    uint32_t sequence = 0; // assigned by FrameSlots::publish()
};

enum class AbortReason : uint8_t { Oversize, Corrupt, CaptureFailed };

/** @brief Whatever a FrameLease must notify when it is released (FrameSlots natively, FrameStore on device). */
class SlotReleaser
{
public:
    virtual void release_slot(size_t slot) = 0;

protected:
    ~SlotReleaser() = default;
};

/** @brief Move-only handle on a published frame. Releases its slot exactly once, on release() or destruction. */
class FrameLease
{
public:
    FrameLease(SlotReleaser& owner, size_t slot, const FrameMeta& meta) : owner_(&owner), slot_(slot), meta_(meta) {}

    FrameLease(const FrameLease&) = delete;
    FrameLease& operator=(const FrameLease&) = delete;

    FrameLease(FrameLease&& other) noexcept
        : owner_(std::exchange(other.owner_, nullptr)), slot_(other.slot_), meta_(other.meta_)
    {
    }

    FrameLease& operator=(FrameLease&& other) noexcept
    {
        if (this != &other)
        {
            release();
            owner_ = std::exchange(other.owner_, nullptr);
            slot_ = other.slot_;
            meta_ = other.meta_;
        }
        return *this;
    }

    ~FrameLease() { release(); }

    void release()
    {
        if (owner_ != nullptr)
        {
            owner_->release_slot(slot_);
            owner_ = nullptr;
        }
    }

    size_t slot() const { return slot_; }
    const FrameMeta& meta() const { return meta_; }
    bool valid() const { return owner_ != nullptr; }

private:
    SlotReleaser* owner_;
    size_t slot_;
    FrameMeta meta_;
};

class FrameSlots : public SlotReleaser
{
public:
    /** @brief A slot that is not latest, not leased and not being written; nullopt (and a counted drop) if none. */
    std::optional<size_t> begin_write()
    {
        for (size_t i = 0; i < kSlotCount; ++i)
        {
            if (slots_[i].state == State::Free)
            {
                slots_[i].state = State::Writing;
                return i;
            }
        }
        ++dropped_no_slot_;
        return std::nullopt;
    }

    /** @brief Make a written slot the latest frame. Assigns the next sequence number. */
    void publish(size_t slot, FrameMeta meta)
    {
        meta.sequence = ++sequence_;
        slots_[slot].state = State::Published;
        slots_[slot].meta = meta;
        if (latest_ && *latest_ != slot && slots_[*latest_].readers == 0)
        {
            slots_[*latest_].state = State::Free;
        }
        latest_ = slot;
        ++published_;
    }

    /** @brief Give up a slot taken by begin_write(). Oversize and Corrupt are counted. */
    void abort_write(size_t slot, AbortReason reason)
    {
        slots_[slot].state = State::Free;
        if (reason == AbortReason::Oversize)
        {
            ++dropped_oversize_;
        } else if (reason == AbortReason::Corrupt)
        {
            ++dropped_corrupt_;
        }
    }

    std::optional<FrameLease> acquire_latest() { return acquire_latest(*this); }

    /** @brief Lease the latest frame; the lease will call releaser.release_slot() when done. */
    std::optional<FrameLease> acquire_latest(SlotReleaser& releaser)
    {
        if (!latest_)
        {
            return std::nullopt;
        }
        Slot& slot = slots_[*latest_];
        ++slot.readers;
        return FrameLease(releaser, *latest_, slot.meta);
    }

    void release_slot(size_t slot) override
    {
        Slot& s = slots_[slot];
        if (s.readers > 0)
        {
            --s.readers;
        }
        if (s.readers == 0 && s.state == State::Published && latest_ != slot)
        {
            s.state = State::Free;
        }
    }

    uint32_t frames_published() const { return published_; }
    uint32_t frames_dropped_no_slot() const { return dropped_no_slot_; }
    uint32_t frames_dropped_oversize() const { return dropped_oversize_; }
    uint32_t frames_dropped_corrupt() const { return dropped_corrupt_; }

private:
    enum class State : uint8_t { Free, Writing, Published };

    struct Slot {
        State state = State::Free;
        uint8_t readers = 0;
        FrameMeta meta;
    };

    std::array<Slot, kSlotCount> slots_{};
    std::optional<size_t> latest_;
    uint32_t sequence_ = 0;
    uint32_t published_ = 0;
    uint32_t dropped_no_slot_ = 0;
    uint32_t dropped_oversize_ = 0;
    uint32_t dropped_corrupt_ = 0;
};
} // namespace frames
