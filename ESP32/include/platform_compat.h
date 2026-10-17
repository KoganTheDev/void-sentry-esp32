#pragma once

/**
 * @file platform_compat.h
 * @brief The only place where Arduino core 2 (ESP-IDF 4.4) and core 3 (ESP-IDF 5.x) APIs differ.
 * @details Stage 7 side-by-side phase: every source builds on both cores; core-version `#if`s live here and
 *          nowhere else. Deleted at cut-over, keeping only the core 3 branches.
 *          (JPEG decoding needs no shim: both cores use the vendored C TJpgDec in lib/tjpgd, see tjpgd_c.h.)
 */

#include <Arduino.h>
#include <esp_arduino_version.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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
} // namespace compat
