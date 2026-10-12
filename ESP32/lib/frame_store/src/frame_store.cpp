#include "frame_store.h"

#include <esp_heap_caps.h>
#include <esp_log.h>

static const char* TAG_STORE = "FRAME_STORE";

namespace
{
/** @brief Scoped FreeRTOS mutex lock. */
class Lock
{
public:
    explicit Lock(SemaphoreHandle_t mutex) : mutex_(mutex) { xSemaphoreTake(mutex_, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(mutex_); }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;

private:
    SemaphoreHandle_t mutex_;
};
} // namespace

bool FrameStore::begin()
{
    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr)
    {
        ESP_LOGE(TAG_STORE, "Failed to create mutex");
        return false;
    }
    for (uint8_t*& buffer : buffers_)
    {
        buffer = static_cast<uint8_t*>(heap_caps_malloc(kSlotBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (buffer == nullptr)
        {
            ESP_LOGE(TAG_STORE, "Failed to allocate %u bytes of PSRAM for a frame slot", (unsigned)kSlotBytes);
            return false;
        }
    }
    ESP_LOGI(TAG_STORE, "%u slots x %u bytes allocated in PSRAM", (unsigned)frames::kSlotCount, (unsigned)kSlotBytes);
    return true;
}

std::optional<size_t> FrameStore::begin_write()
{
    Lock lock(mutex_);
    return slots_.begin_write();
}

void FrameStore::publish(size_t slot, const frames::FrameMeta& meta)
{
    {
        Lock lock(mutex_);
        slots_.publish(slot, meta);
    }
    if (subscriber_ != nullptr)
    {
        xTaskNotifyGive(subscriber_);
    }
}

void FrameStore::abort_write(size_t slot, frames::AbortReason reason)
{
    Lock lock(mutex_);
    slots_.abort_write(slot, reason);
}

std::optional<frames::FrameLease> FrameStore::acquire_latest()
{
    Lock lock(mutex_);
    return slots_.acquire_latest(*this);
}

void FrameStore::release_slot(size_t slot)
{
    Lock lock(mutex_);
    slots_.release_slot(slot);
}

FrameStoreStats FrameStore::stats()
{
    Lock lock(mutex_);
    FrameStoreStats s;
    s.published = slots_.frames_published();
    s.dropped_no_slot = slots_.frames_dropped_no_slot();
    s.dropped_oversize = slots_.frames_dropped_oversize();
    s.dropped_corrupt = slots_.frames_dropped_corrupt();
    return s;
}
