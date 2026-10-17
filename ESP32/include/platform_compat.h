#pragma once

/**
 * @file platform_compat.h
 * @brief The only place where Arduino core 2 (ESP-IDF 4.4) and core 3 (ESP-IDF 5.x) APIs differ.
 * @details Stage 7 side-by-side phase: every source builds on both cores; core-version `#if`s live here and
 *          nowhere else. Deleted at cut-over, keeping only the core 3 branches.
 */

#include <Arduino.h>
#include <cstddef>
#include <cstdint>
#include <esp_arduino_version.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#if ESP_ARDUINO_VERSION_MAJOR >= 3
#include <esp32/rom/tjpgd.h>
#else
#include <esp_jpg_decode.h>
#endif

namespace compat
{
/// (always_inline: called from an IRAM tick hook.)
/// FreeRTOS: IDF 5 renamed xTaskGetIdleTaskHandleForCPU to ...ForCore (core 2 keeps the CPU name).
__attribute__((always_inline)) inline TaskHandle_t idle_task_for_core(BaseType_t core)
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    return xTaskGetIdleTaskHandleForCore(core);
#else
    return xTaskGetIdleTaskHandleForCPU(core);
#endif
}

/// FreeRTOS: IDF 5 renamed xTaskGetCurrentTaskHandleForCPU to ...ForCore.
__attribute__((always_inline)) inline TaskHandle_t current_task_for_core(BaseType_t core)
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    return xTaskGetCurrentTaskHandleForCore(core);
#else
    return xTaskGetCurrentTaskHandleForCPU(core);
#endif
}

/// Reader callback: copy `len` bytes at `index` into `buf` (buf == nullptr means skip); returns bytes consumed.
using JpgReader = size_t (*)(void* arg, size_t index, uint8_t* buf, size_t len);
/// Writer callback: data == nullptr announces the scaled output size in w/h (x == y == 0); otherwise an RGB888 block.
using JpgWriter = bool (*)(void* arg, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t* data);

/**
 * @brief Decode a JPEG at 1/8 scale through the ROM TJpgDec, one RGB888 block per writer call.
 * @details esp32-camera on core 3 no longer ships esp_jpg_decode.h (only esp_jpeg, which needs an output buffer),
 *          so core 3 drives the ROM decoder directly, exactly as the old esp_jpg_decode did (3100-byte work pool).
 *          Not reentrant: the work pool is static (one decoder task only).
 */
inline esp_err_t jpg_decode_8x(size_t len, JpgReader reader, JpgWriter writer, void* arg)
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    struct Io {
        JpgReader reader;
        JpgWriter writer;
        void* arg;
        size_t index;
    } io{reader, writer, arg, 0};
    static uint8_t work[3100] __attribute__((aligned(4)));
    (void)len;
    JDEC jd;
    auto read_cb = [](JDEC* d, BYTE* buf, UINT n) -> UINT {
        auto* p = static_cast<Io*>(d->device);
        const size_t got = p->reader(p->arg, p->index, buf, n);
        p->index += got;
        return static_cast<UINT>(got);
    };
    auto write_cb = [](JDEC* d, void* bitmap, JRECT* r) -> UINT {
        auto* p = static_cast<Io*>(d->device);
        const uint16_t w = static_cast<uint16_t>(r->right - r->left + 1);
        const uint16_t h = static_cast<uint16_t>(r->bottom - r->top + 1);
        return p->writer(p->arg, r->left, r->top, w, h, static_cast<uint8_t*>(bitmap)) ? 1 : 0;
    };
    if (jd_prepare(&jd, read_cb, work, sizeof(work), &io) != JDR_OK)
    {
        return ESP_FAIL;
    }
    constexpr uint8_t kScale8 = 3;
    if (!writer(arg, 0, 0, static_cast<uint16_t>(jd.width >> kScale8), static_cast<uint16_t>(jd.height >> kScale8),
                nullptr))
    {
        return ESP_FAIL;
    }
    return jd_decomp(&jd, write_cb, kScale8) == JDR_OK ? ESP_OK : ESP_FAIL;
#else
    return esp_jpg_decode(len, JPG_SCALE_8X, reader, writer, arg);
#endif
}
} // namespace compat
