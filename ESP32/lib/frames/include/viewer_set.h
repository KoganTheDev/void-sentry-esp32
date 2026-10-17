/**
 * @file viewer_set.h
 * @brief Which stream viewers should receive which frame (pure C++20, natively tested).
 * @details A viewer that is still receiving (busy) is skipped, so it never builds a backlog and always gets the
 * newest frame next - this is what keeps stream latency low. NOT thread-safe; StreamBroadcaster locks around it.
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace frames
{
constexpr size_t kMaxViewers = 2;

/** @brief Fixed-capacity result so the stream path never allocates per frame. */
struct ReadyList {
    std::array<int, kMaxViewers> fds{};
    size_t count = 0;
};

class ViewerSet
{
public:
    /** @brief Add a viewer by socket fd. Adding an existing fd is a no-op. False when full. */
    bool add(int fd)
    {
        if (index_of(fd))
        {
            return true;
        }
        for (Viewer& v : viewers_)
        {
            if (!v.used)
            {
                v = Viewer{true, fd, false, 0};
                return true;
            }
        }
        return false;
    }

    void remove(int fd)
    {
        if (auto i = index_of(fd))
        {
            viewers_[*i] = Viewer{};
        }
    }

    /** @brief Viewers that are idle and have not yet been sent this sequence (or newer). */
    ReadyList ready_for(uint32_t sequence) const
    {
        ReadyList ready;
        for (const Viewer& v : viewers_)
        {
            if (v.used && !v.busy && v.last_sequence_sent < sequence)
            {
                ready.fds[ready.count++] = v.fd;
            }
        }
        return ready;
    }

    void mark_sending(int fd, uint32_t sequence)
    {
        if (auto i = index_of(fd))
        {
            viewers_[*i].busy = true;
            viewers_[*i].last_sequence_sent = sequence;
        }
    }

    void mark_done(int fd)
    {
        if (auto i = index_of(fd))
        {
            viewers_[*i].busy = false;
        }
    }

    std::optional<size_t> index_of(int fd) const
    {
        for (size_t i = 0; i < kMaxViewers; ++i)
        {
            if (viewers_[i].used && viewers_[i].fd == fd)
            {
                return i;
            }
        }
        return std::nullopt;
    }

    size_t size() const
    {
        size_t n = 0;
        for (const Viewer& v : viewers_)
        {
            n += v.used ? 1 : 0;
        }
        return n;
    }

private:
    struct Viewer {
        bool used = false;
        int fd = -1;
        bool busy = false;
        uint32_t last_sequence_sent = 0;
    };

    std::array<Viewer, kMaxViewers> viewers_{};
};
} // namespace frames
