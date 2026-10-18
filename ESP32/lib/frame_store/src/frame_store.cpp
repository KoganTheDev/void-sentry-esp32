#include "frame_store.h"

#include "sync.h"

#include <esp32-hal-log.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

static const char* TAG_STORE = "FRAME_STORE";

bool FrameStore::begin()
{
    if (ready_)
    {
        return true;
    }
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
            for (uint8_t*& allocated : buffers_)
            {
                if (allocated != nullptr)
                {
                    heap_caps_free(allocated);
                    allocated = nullptr;
                }
            }
            vSemaphoreDelete(mutex_);
            mutex_ = nullptr;
            return false;
        }
    }
    ESP_LOGI(TAG_STORE, "%u slots x %u bytes allocated in PSRAM", (unsigned)frames::kSlotCount, (unsigned)kSlotBytes);
    ready_ = true;
    return true;
}

std::optional<size_t> FrameStore::begin_write()
{
    if (!ready_)
    {
        return std::nullopt;
    }
    vsync::MutexLock lock(mutex_);
    return slots_.begin_write();
}

void FrameStore::publish(size_t slot, const frames::FrameMeta& meta)
{
    if (!ready_)
    {
        return;
    }
    {
        vsync::MutexLock lock(mutex_);
        slots_.publish(slot, meta);
    }
    for (const std::atomic<TaskHandle_t>& slot : this->subscribers_)
    {
        TaskHandle_t task = slot.load();
        if (task != nullptr)
        {
            xTaskNotifyGive(task);
        }
    }
}

bool FrameStore::add_subscriber(TaskHandle_t task)
{
    if (task == nullptr)
    {
        return false;
    }
    for (const std::atomic<TaskHandle_t>& slot : this->subscribers_)
    {
        if (slot.load() == task)
        {
            return true; // already registered
        }
    }
    for (std::atomic<TaskHandle_t>& slot : this->subscribers_)
    {
        TaskHandle_t expected = nullptr;
        if (slot.compare_exchange_strong(expected, task))
        {
            return true;
        }
    }
    return false;
}

void FrameStore::remove_subscriber(TaskHandle_t task)
{
    if (task == nullptr)
    {
        return;
    }
    for (std::atomic<TaskHandle_t>& slot : this->subscribers_)
    {
        TaskHandle_t expected = task;
        slot.compare_exchange_strong(expected, nullptr);
    }
}

void FrameStore::abort_write(size_t slot, frames::AbortReason reason)
{
    if (!ready_)
    {
        return;
    }
    vsync::MutexLock lock(mutex_);
    slots_.abort_write(slot, reason);
}

std::optional<frames::FrameLease> FrameStore::acquire_latest()
{
    if (!ready_)
    {
        return std::nullopt;
    }
    vsync::MutexLock lock(mutex_);
    return slots_.acquire_latest(*this);
}

void FrameStore::release_slot(size_t slot)
{
    if (!ready_)
    {
        return;
    }
    vsync::MutexLock lock(mutex_);
    slots_.release_slot(slot);
}

TaskHandle_t FrameStore::start_subscriber_task(TaskFunction_t entry, const char* name, uint32_t stack_bytes,
                                               UBaseType_t priority, BaseType_t core, void* arg)
{
    TaskHandle_t task = nullptr;
    if (xTaskCreatePinnedToCore(entry, name, stack_bytes, arg, priority, &task, core) != pdPASS)
    {
        ESP_LOGE(TAG_STORE, "Failed to create the %s task", name);
        return nullptr;
    }
    if (!this->add_subscriber(task))
    {
        ESP_LOGE(TAG_STORE, "FrameStore subscriber slots are full");
        vTaskDelete(task);
        return nullptr;
    }
    return task;
}

FrameStoreStats FrameStore::stats()
{
    if (!ready_)
    {
        return FrameStoreStats{};
    }
    vsync::MutexLock lock(mutex_);
    FrameStoreStats s;
    s.published = slots_.frames_published();
    s.dropped_no_slot = slots_.frames_dropped_no_slot();
    s.dropped_oversize = slots_.frames_dropped_oversize();
    s.dropped_corrupt = slots_.frames_dropped_corrupt();
    return s;
}
