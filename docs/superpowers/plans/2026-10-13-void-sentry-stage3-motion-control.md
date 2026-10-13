# Stage 3 Motion Control Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** In AI mode, aim the turret (stepper pan, servo tilt) at the tracked target using a calibrated image-to-angle map, with a non-blocking motor task. Add joystick 2-point calibration that is saved to NVS.

**Architecture:**
- **Pure library `lib/aiming`** (natively tested): the calibration map plus its CRC'd blob, a trapezoidal step planner, the aim policy (hold/aim/home), a button classifier (short/long) and a calibration session state machine.
- **Device side:**
  - A `Turret` motor task on core 0 owns a hand-written 4-phase coil driver and the servo.
  - The `Controller` gets a third mode (CALIBRATE) and drives the turret through absolute setpoints or jog speeds.
  - Mode, calibration and angles flow to `/commands`, the HUD and the stats line.

**Tech Stack:** C++17, PlatformIO (espressif32@6.12.0, Arduino core 2.0.17, GCC 8.4; host g++ >= 8), FreeRTOS, ESP32Servo, Arduino `Preferences` (NVS), Unity.

**Spec:** `docs/superpowers/specs/2026-10-13-void-sentry-stage3-motion-control-design.md`

## Global Constraints

- **Language:** C++17 everywhere; no C++20.
- **`lib/aiming` purity:** no Arduino, ESP-IDF or FreeRTOS headers, and no heap allocation. It may include `tracking_types.h` from `lib/tracking`, which is pure.
- **Stepper geometry:** 2048 full steps per revolution. Pan 0 = home = the boot position, positive = right. Pan soft limits ±90° (±512 steps).
- **Tilt:** servo degrees, home 90, soft limits [20, 160], Up = larger angle.
- **Image:** QVGA, 320x240 px.
- **Planner defaults:**
  - max 400 steps/s
  - accel 800 steps/s²
  - min (creep) speed 20 steps/s
  - 1 kHz tick
- **Policy defaults:** deadband 1°, home delay 2000 ms.
- **Calibration:**
  - Minimum point separation 80 px in x and 60 px in y.
  - Default map: horizontal FOV 66° over 320 px, vertical FOV 50° over 240 px. Image centre (160, 120) maps to pan 0, tilt 90.
- **Button:** debounce 50 ms, long press at 1500 ms.
- **Motor task:** core 0, priority 3, 1 ms period.
  - Coils are released after 500 ms idle.
  - Servo is rate-limited to 120°/s and written at 50 Hz.
  - Full-step coil sequence on (IN1, IN2, IN3, IN4): `1100, 0110, 0011, 1001`; a positive step advances the phase.
- **Style:**
  - Never use em dashes in code comments, docs or commit messages; use " - ".
  - Pure-lib members take a trailing underscore; device classes take a leading underscore and use `this->`.
  - Doxygen `@brief` on public headers.
  - clang-format 19 (the hook formats on edit; CI checks).
- **Never read or print `ESP32/include/secrets.h`**, and never run a shell command containing the substring `secret`.
- **`pio`:**
  - It is `~/.platformio/penv/Scripts/pio.exe`, run from `ESP32/`.
  - Never pipe it into `head`/`grep -m1`; redirect to a log file instead.
  - The native env needs WinLibs GCC 14 first on PATH: `export PATH="$(ls -d /c/Users/yuval/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs*/mingw64/bin | head -1):$PATH"`.
- **Commit trailer:** `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **Target changes mid-move.** A reversal at full speed must decelerate before turning around, then land exactly on the new target with no oscillation. Pinned by `test_reversal_decelerates_then_arrives` (Task 2).
2. **Soft limits in jog mode.** Holding the joystick must never drive the pan past ±512 steps. Pinned by `test_jog_stops_at_limit` (Task 2).
3. **Calibration press with no visible target.** It is ignored and does not advance the session. Pinned by `test_record_without_target_is_ignored` (Task 3).
4. **Corrupted or old NVS data.** It falls back to the default map instead of aiming wildly. Pinned by `test_blob_rejects_*` (Task 1).
5. **Lost target.** Aiming holds for 2 s, then homes exactly once, and aims again on re-lock. Pinned by `test_idle_holds_then_homes_once` and `test_relock_after_home_aims` (Task 3).

---

## File map

| File | Task | Responsibility |
|---|---|---|
| `ESP32/lib/aiming/include/aim_map.h`, `src/aim_map.cpp` | 1 | `Angles`, `CalPoint`, `AimMap`, `default_map`, `fit`, clamping, deg<->steps |
| `ESP32/lib/aiming/include/calibration_blob.h`, `src/calibration_blob.cpp` | 1 | 28-byte versioned, CRC-32 blob |
| `ESP32/test/native/test_aim_map/test_aim_map.cpp` | 1 | Tests for the map and the blob |
| `ESP32/lib/aiming/include/step_planner.h`, `src/step_planner.cpp` | 2 | Trapezoidal 1 kHz planner (target + jog) |
| `ESP32/test/native/test_step_planner/test_step_planner.cpp` | 2 | Planner tests |
| `ESP32/lib/aiming/include/aim_policy.h`, `src/aim_policy.cpp` | 3 | Hold / Aim / Home policy |
| `ESP32/lib/aiming/include/button_classifier.h`, `src/button_classifier.cpp` | 3 | Short/long press |
| `ESP32/lib/aiming/include/calibration_session.h`, `src/calibration_session.cpp` | 3 | 2-point calibration state machine |
| `ESP32/test/native/test_aim_policy/test_aim_policy.cpp` | 3 | Policy, button, session and pipeline tests |
| `ESP32/lib/motion/include/base_turret.h`, `include/turret.h`, `src/turret.cpp` | 4 | Motor task, coil driver, servo |
| `ESP32/lib/motion/include/movement_manager.h`, `base_movement_manager.h`, `src/movement_manager.cpp` | 4 | Deleted |
| `ESP32/platformio.ini` | 4 | Drop the `Stepper` lib_dep |
| `ESP32/lib/controller/*`, `ESP32/include/system_control_types.h` | 5 | Modes, aiming, calibration, NVS store, status |
| `ESP32/lib/network/*`, `ESP32/src/main.cpp`, `web/web_ui.html` | 5 | `/commands` JSON, wiring, stats, HUD |
| `CLAUDE.md` | 5 | Docs |

---

### Task 1: AimMap and calibration blob

**Files:**
- Create: `ESP32/lib/aiming/include/aim_map.h`
- Create: `ESP32/lib/aiming/src/aim_map.cpp`
- Create: `ESP32/lib/aiming/include/calibration_blob.h`
- Create: `ESP32/lib/aiming/src/calibration_blob.cpp`
- Test: `ESP32/test/native/test_aim_map/test_aim_map.cpp`

**Interfaces:**
- Produces, in namespace `aiming`:
  - Constants: `kStepsPerRev`, `kPanMinDeg`, `kPanMaxDeg`, `kTiltMinDeg`, `kTiltMaxDeg`, `kTiltHomeDeg`, `kFrameW`, `kFrameH`, `kMinCalDx`, `kMinCalDy`.
  - Types: `struct Angles {float pan; float tilt;}`, `struct CalPoint {float x, y, pan, tilt;}`.
  - `struct AimMap {float pan_a, pan_b, tilt_a, tilt_b; Angles aim(float x, float y) const;}`.
  - Functions: `AimMap default_map()`, `std::optional<AimMap> fit(const CalPoint&, const CalPoint&)`, `Angles clamp_angles(Angles)`, `int32_t pan_deg_to_steps(float)`, `float pan_steps_to_deg(int32_t)`.
  - Blob: `constexpr size_t kBlobSize = 28`, `using Blob = std::array<uint8_t, kBlobSize>`, `Blob serialize(const AimMap&)`, `std::optional<AimMap> deserialize(const uint8_t*, size_t)`, `uint32_t crc32(const uint8_t*, size_t)`.

- [ ] **Step 1: Write the failing tests** `ESP32/test/native/test_aim_map/test_aim_map.cpp`

```cpp
/**
 * @file test_aim_map.cpp
 * @brief Host-side tests for the image-to-angle map, its 2-point fit, and the NVS calibration blob.
 */

#include <unity.h>

#include "aim_map.h"
#include "calibration_blob.h"

#include <cmath>
#include <cstring>
#include <limits>

using namespace aiming;

void setUp(void) {}
void tearDown(void) {}

void test_default_map_centre_is_home(void)
{
    const Angles a = default_map().aim(160.0f, 120.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, a.pan);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, kTiltHomeDeg, a.tilt);
}

void test_default_map_edges(void)
{
    const AimMap m = default_map();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 33.0f, m.aim(320.0f, 120.0f).pan);   // right edge: +half FOV
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -33.0f, m.aim(0.0f, 120.0f).pan);    // left edge
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 115.0f, m.aim(160.0f, 0.0f).tilt);   // top of image: tilt up
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 65.0f, m.aim(160.0f, 240.0f).tilt);  // bottom: tilt down
}

void test_fit_reproduces_points_and_is_linear(void)
{
    const CalPoint p1{40.0f, 50.0f, -30.0f, 110.0f};
    const CalPoint p2{280.0f, 200.0f, 25.0f, 70.0f};
    const auto m = fit(p1, p2);
    TEST_ASSERT_TRUE(m.has_value());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -30.0f, m->aim(40.0f, 50.0f).pan);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 110.0f, m->aim(40.0f, 50.0f).tilt);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, m->aim(280.0f, 200.0f).pan);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 70.0f, m->aim(280.0f, 200.0f).tilt);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -2.5f, m->aim(160.0f, 125.0f).pan); // midpoint
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 90.0f, m->aim(160.0f, 125.0f).tilt);
}

void test_fit_rejects_points_too_close(void)
{
    TEST_ASSERT_FALSE(fit({100, 50, 0, 100}, {170, 200, 10, 70}).has_value()); // dx 70 < 80
    TEST_ASSERT_FALSE(fit({40, 100, 0, 100}, {280, 150, 10, 70}).has_value()); // dy 50 < 60
    TEST_ASSERT_TRUE(fit({40, 100, 0, 100}, {120, 160, 10, 70}).has_value());  // exactly 80 / 60
}

void test_fit_rejects_non_finite(void)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    TEST_ASSERT_FALSE(fit({40, 50, nan, 100}, {280, 200, 10, 70}).has_value());
}

void test_aim_is_clamped_to_soft_limits(void)
{
    const AimMap steep{2.0f, -320.0f, -2.0f, 330.0f};
    const Angles lo = steep.aim(0.0f, 240.0f);
    const Angles hi = steep.aim(320.0f, 0.0f);
    TEST_ASSERT_EQUAL_FLOAT(kPanMinDeg, lo.pan);
    TEST_ASSERT_EQUAL_FLOAT(kTiltMinDeg, lo.tilt);
    TEST_ASSERT_EQUAL_FLOAT(kPanMaxDeg, hi.pan);
    TEST_ASSERT_EQUAL_FLOAT(kTiltMaxDeg, hi.tilt);
}

void test_deg_steps_round_trip(void)
{
    TEST_ASSERT_EQUAL_INT32(512, pan_deg_to_steps(90.0f));
    TEST_ASSERT_EQUAL_INT32(-512, pan_deg_to_steps(-90.0f));
    TEST_ASSERT_EQUAL_INT32(6, pan_deg_to_steps(1.0f)); // 5.69 rounds to 6
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 90.0f, pan_steps_to_deg(512));
}

void test_crc32_known_vector(void)
{
    const char* s = "123456789";
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, crc32(reinterpret_cast<const uint8_t*>(s), 9));
}

void test_blob_round_trip(void)
{
    const AimMap m{0.25f, -40.0f, -0.2f, 112.0f};
    const Blob b = serialize(m);
    const auto back = deserialize(b.data(), b.size());
    TEST_ASSERT_TRUE(back.has_value());
    TEST_ASSERT_EQUAL_FLOAT(m.pan_a, back->pan_a);
    TEST_ASSERT_EQUAL_FLOAT(m.pan_b, back->pan_b);
    TEST_ASSERT_EQUAL_FLOAT(m.tilt_a, back->tilt_a);
    TEST_ASSERT_EQUAL_FLOAT(m.tilt_b, back->tilt_b);
}

void test_blob_rejects_wrong_length(void)
{
    const Blob b = serialize(default_map());
    TEST_ASSERT_FALSE(deserialize(b.data(), b.size() - 1).has_value());
    TEST_ASSERT_FALSE(deserialize(nullptr, kBlobSize).has_value());
}

void test_blob_rejects_bad_magic_version_and_crc(void)
{
    Blob b = serialize(default_map());
    Blob bad_magic = b;
    bad_magic[0] ^= 0xFF;
    TEST_ASSERT_FALSE(deserialize(bad_magic.data(), bad_magic.size()).has_value());

    Blob bad_version = b;
    bad_version[4] = 9; // version field (little-endian uint16 at offset 4)
    TEST_ASSERT_FALSE(deserialize(bad_version.data(), bad_version.size()).has_value());

    Blob bad_payload = b;
    bad_payload[10] ^= 0x01; // inside the floats: CRC must catch it
    TEST_ASSERT_FALSE(deserialize(bad_payload.data(), bad_payload.size()).has_value());
}

void test_blob_rejects_non_finite_values(void)
{
    AimMap m = default_map();
    m.tilt_b = std::numeric_limits<float>::infinity();
    const Blob b = serialize(m); // CRC is valid, value is not
    TEST_ASSERT_FALSE(deserialize(b.data(), b.size()).has_value());
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_default_map_centre_is_home);
    RUN_TEST(test_default_map_edges);
    RUN_TEST(test_fit_reproduces_points_and_is_linear);
    RUN_TEST(test_fit_rejects_points_too_close);
    RUN_TEST(test_fit_rejects_non_finite);
    RUN_TEST(test_aim_is_clamped_to_soft_limits);
    RUN_TEST(test_deg_steps_round_trip);
    RUN_TEST(test_crc32_known_vector);
    RUN_TEST(test_blob_round_trip);
    RUN_TEST(test_blob_rejects_wrong_length);
    RUN_TEST(test_blob_rejects_bad_magic_version_and_crc);
    RUN_TEST(test_blob_rejects_non_finite_values);
    return UNITY_END();
}
```

Checks for the midpoint values:
- `pan_a = 55/240`, so pan at x = 160 is `-30 + 120 * 55/240 = -2.5`.
- `tilt_a = -40/150`, so tilt at y = 125 is `110 - 75 * 40/150 = 90`.

- [ ] **Step 2: Run to confirm it fails.** From `ESP32/`: `pio test -e native -f native/test_aim_map > <scratch>/a1.log 2>&1`. Expect `aim_map.h: No such file`.

- [ ] **Step 3: Create `aim_map.h`**

```cpp
/**
 * @file aim_map.h
 * @brief Maps a target's image position to turret angles (pure C++17, natively tested).
 * @details The camera is fixed (not on the turret), so aiming is open-loop: pan depends linearly on image x and tilt
 * on image y. The map comes from a 2-point joystick calibration, or from default_map() (camera next to the turret,
 * OV3660 field of view) until one exists. Angles: pan degrees from home (positive = right), tilt servo degrees
 * (larger = up).
 */

#pragma once

#include <cstdint>
#include <optional>

namespace aiming
{
constexpr int32_t kStepsPerRev = 2048; ///< 28BYJ-48 full steps per revolution
constexpr float kPanMinDeg = -90.0f;
constexpr float kPanMaxDeg = 90.0f;
constexpr float kTiltMinDeg = 20.0f;
constexpr float kTiltMaxDeg = 160.0f;
constexpr float kTiltHomeDeg = 90.0f;
constexpr int kFrameW = 320; ///< QVGA
constexpr int kFrameH = 240;
constexpr float kMinCalDx = 80.0f; ///< closer calibration points amplify aiming error
constexpr float kMinCalDy = 60.0f;

struct Angles {
    float pan = 0.0f;
    float tilt = kTiltHomeDeg;
};

/** @brief One calibration sample: the target's pixel and the turret angles that pointed at it. */
struct CalPoint {
    float x = 0.0f;
    float y = 0.0f;
    float pan = 0.0f;
    float tilt = kTiltHomeDeg;
};

/** @brief pan = pan_a * x + pan_b, tilt = tilt_a * y + tilt_b. */
struct AimMap {
    float pan_a = 0.0f;
    float pan_b = 0.0f;
    float tilt_a = 0.0f;
    float tilt_b = kTiltHomeDeg;

    /** @brief Angles for an image position, clamped to the soft limits. */
    Angles aim(float x, float y) const;
};

/** @brief Camera next to the turret, 66 x 50 degree field of view, image centre = home. */
AimMap default_map();

/** @brief Per-axis line through two calibration points; empty if they are too close or not finite. */
std::optional<AimMap> fit(const CalPoint& p1, const CalPoint& p2);

Angles clamp_angles(Angles a);
int32_t pan_deg_to_steps(float deg);
float pan_steps_to_deg(int32_t steps);
} // namespace aiming
```

- [ ] **Step 4: Create `aim_map.cpp`**

```cpp
#include "aim_map.h"

#include <algorithm>
#include <cmath>

namespace aiming
{
namespace
{
constexpr float kHFovDeg = 66.0f;
constexpr float kVFovDeg = 50.0f;

bool finite(float v) { return std::isfinite(v); }
} // namespace

Angles clamp_angles(Angles a)
{
    a.pan = std::clamp(a.pan, kPanMinDeg, kPanMaxDeg);
    a.tilt = std::clamp(a.tilt, kTiltMinDeg, kTiltMaxDeg);
    return a;
}

Angles AimMap::aim(float x, float y) const
{
    return clamp_angles(Angles{pan_a * x + pan_b, tilt_a * y + tilt_b});
}

AimMap default_map()
{
    AimMap m;
    m.pan_a = kHFovDeg / kFrameW;   // image right -> pan right
    m.pan_b = -m.pan_a * (kFrameW / 2.0f);
    m.tilt_a = -kVFovDeg / kFrameH; // image down -> tilt down (smaller servo angle)
    m.tilt_b = kTiltHomeDeg - m.tilt_a * (kFrameH / 2.0f);
    return m;
}

std::optional<AimMap> fit(const CalPoint& p1, const CalPoint& p2)
{
    for (float v : {p1.x, p1.y, p1.pan, p1.tilt, p2.x, p2.y, p2.pan, p2.tilt})
    {
        if (!finite(v))
        {
            return std::nullopt;
        }
    }
    const float dx = p2.x - p1.x;
    const float dy = p2.y - p1.y;
    if (std::fabs(dx) < kMinCalDx || std::fabs(dy) < kMinCalDy)
    {
        return std::nullopt;
    }
    AimMap m;
    m.pan_a = (p2.pan - p1.pan) / dx;
    m.pan_b = p1.pan - m.pan_a * p1.x;
    m.tilt_a = (p2.tilt - p1.tilt) / dy;
    m.tilt_b = p1.tilt - m.tilt_a * p1.y;
    return m;
}

int32_t pan_deg_to_steps(float deg)
{
    return static_cast<int32_t>(std::lround(deg * static_cast<float>(kStepsPerRev) / 360.0f));
}

float pan_steps_to_deg(int32_t steps) { return static_cast<float>(steps) * 360.0f / static_cast<float>(kStepsPerRev); }
} // namespace aiming
```

- [ ] **Step 5: Create `calibration_blob.h` / `.cpp`**

```cpp
/**
 * @file calibration_blob.h
 * @brief Versioned, CRC-checked byte layout for saving an AimMap to NVS (pure C++17, natively tested).
 * @details Layout (little-endian, 28 bytes): magic "AIM1" (u32), version (u16, = 1), reserved (u16, 0),
 * pan_a, pan_b, tilt_a, tilt_b (4 x f32), CRC-32 of the first 24 bytes (u32). Both the ESP32 and x86 are
 * little-endian, so fields are copied with memcpy. Anything that fails a check deserializes to nothing, and the
 * caller falls back to default_map().
 */

#pragma once

#include "aim_map.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace aiming
{
constexpr size_t kBlobSize = 28;
using Blob = std::array<uint8_t, kBlobSize>;

uint32_t crc32(const uint8_t* data, size_t len);
Blob serialize(const AimMap& map);
std::optional<AimMap> deserialize(const uint8_t* data, size_t len);
} // namespace aiming
```

```cpp
#include "calibration_blob.h"

#include <cmath>
#include <cstring>

namespace aiming
{
namespace
{
constexpr uint32_t kMagic = 0x314D4941u; // "AIM1" little-endian
constexpr uint16_t kVersion = 1;
constexpr size_t kCrcOffset = kBlobSize - 4;
} // namespace

uint32_t crc32(const uint8_t* data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

Blob serialize(const AimMap& map)
{
    Blob b{};
    const uint16_t reserved = 0;
    const float values[4] = {map.pan_a, map.pan_b, map.tilt_a, map.tilt_b};
    std::memcpy(b.data(), &kMagic, 4);
    std::memcpy(b.data() + 4, &kVersion, 2);
    std::memcpy(b.data() + 6, &reserved, 2);
    std::memcpy(b.data() + 8, values, 16);
    const uint32_t crc = crc32(b.data(), kCrcOffset);
    std::memcpy(b.data() + kCrcOffset, &crc, 4);
    return b;
}

std::optional<AimMap> deserialize(const uint8_t* data, size_t len)
{
    if (!data || len != kBlobSize)
    {
        return std::nullopt;
    }
    uint32_t magic = 0;
    uint16_t version = 0;
    uint32_t stored_crc = 0;
    std::memcpy(&magic, data, 4);
    std::memcpy(&version, data + 4, 2);
    std::memcpy(&stored_crc, data + kCrcOffset, 4);
    if (magic != kMagic || version != kVersion || stored_crc != crc32(data, kCrcOffset))
    {
        return std::nullopt;
    }
    float values[4];
    std::memcpy(values, data + 8, 16);
    for (float v : values)
    {
        if (!std::isfinite(v))
        {
            return std::nullopt;
        }
    }
    return AimMap{values[0], values[1], values[2], values[3]};
}
} // namespace aiming
```

- [ ] **Step 6: Run until it passes.** `pio test -e native -f native/test_aim_map` should report 12 tests, 0 failures. Then run `pio test -e native`: all suites PASSED. Then `pio run -e freenove_esp32_wrover`: SUCCESS.

- [ ] **Step 7: Commit**

```bash
git add ESP32/lib/aiming ESP32/test/native/test_aim_map
git commit -m "Add aim map, 2-point fit and CRC-checked calibration blob

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: StepPlanner

**Files:**
- Create: `ESP32/lib/aiming/include/step_planner.h`
- Create: `ESP32/lib/aiming/src/step_planner.cpp`
- Test: `ESP32/test/native/test_step_planner/test_step_planner.cpp`

**Interfaces:**
- Produces: `struct aiming::PlannerConfig {float max_speed = 400, accel = 800, min_speed = 20; int32_t min_pos = -512, max_pos = 512; float tick_s = 0.001f;}`; `class aiming::StepPlanner { explicit StepPlanner(const PlannerConfig& = {}); void set_target(int32_t); void set_velocity(float steps_per_s); int tick(); int32_t position() const; int32_t target() const; float speed() const; bool is_idle() const; void reset_position(int32_t = 0); const PlannerConfig& config() const; }`.

**Algorithm.** Each tick computes a signed goal speed, moves the current speed toward it by at most `accel * tick_s`, then integrates.

*Goal speed in target mode.* Let `r = target - position`.
- **Arrived:** if `r == 0` and `|v| <= min_speed`, stop: set `v = 0` and clear the fraction.
- **Direction:** `dir = sign(r)`. If `r == 0` while still moving, `dir = -sign(v)`, so the planner comes back.
- **Moving away** (`v * dir < 0`): goal = 0.
- **Close enough to brake** (`|r| <= v²/(2a) + 1`): goal = 0. If braking would drop below the creep speed while still short of the target, hold `dir * min_speed` instead, so the planner always arrives.
- **Otherwise:** goal = `dir * max_speed`, and starting from rest jumps straight to `min_speed`.

*Goal speed in jog mode.*
- Goal = the commanded velocity, clamped to ±max.
- Braking toward a soft limit uses the same rule as braking toward a target.

*Integrate.* `frac += v * tick_s`; when `|frac|` reaches 1, emit one step and subtract it.

*Soft limits.* A step is never emitted past `[min_pos, max_pos]`; at a limit, `v` and `frac` are zeroed.

The only acceleration exception is the start from rest straight to `min_speed`, a 20 steps/s jump the 28BYJ handles fine. The tests allow exactly that.

- [ ] **Step 1: Write the failing tests** `ESP32/test/native/test_step_planner/test_step_planner.cpp`

```cpp
/**
 * @file test_step_planner.cpp
 * @brief Host-side tests for the trapezoidal 1 kHz stepper planner (target and jog modes).
 */

#include <unity.h>

#include "step_planner.h"

#include <cmath>

using namespace aiming;

void setUp(void) {}
void tearDown(void) {}

/** Runs until idle (or max_ticks); checks speed and acceleration limits on every tick. Returns ticks used. */
static int run_checked(StepPlanner& p, int max_ticks, int32_t* min_seen = nullptr, int32_t* max_seen = nullptr,
                       float* peak_speed = nullptr)
{
    const PlannerConfig& c = p.config();
    const float dv = c.accel * c.tick_s;
    float prev = p.speed();
    int ticks = 0;
    for (; ticks < max_ticks; ++ticks)
    {
        const int step = p.tick();
        TEST_ASSERT_TRUE(step >= -1 && step <= 1);
        const float v = p.speed();
        TEST_ASSERT_TRUE(std::fabs(v) <= c.max_speed + 1e-3f);
        const bool creep_jump = std::fabs(prev) <= c.min_speed + 1e-3f && std::fabs(v) <= c.min_speed + 1e-3f;
        TEST_ASSERT_TRUE(std::fabs(v - prev) <= dv + 1e-3f || creep_jump);
        if (min_seen && p.position() < *min_seen)
            *min_seen = p.position();
        if (max_seen && p.position() > *max_seen)
            *max_seen = p.position();
        if (peak_speed && std::fabs(v) > *peak_speed)
            *peak_speed = std::fabs(v);
        prev = v;
        if (p.is_idle())
            break;
    }
    return ticks;
}

void test_zero_distance_stays_idle(void)
{
    StepPlanner p;
    p.set_target(0);
    TEST_ASSERT_EQUAL_INT(0, p.tick());
    TEST_ASSERT_TRUE(p.is_idle());
}

void test_reaches_target_exactly_without_overshoot(void)
{
    StepPlanner p;
    p.set_target(300);
    int32_t max_seen = 0;
    run_checked(p, 5000, nullptr, &max_seen);
    TEST_ASSERT_TRUE(p.is_idle());
    TEST_ASSERT_EQUAL_INT32(300, p.position());
    TEST_ASSERT_EQUAL_INT32(300, max_seen);
}

void test_negative_target(void)
{
    StepPlanner p;
    p.set_target(-200);
    int32_t min_seen = 0;
    run_checked(p, 5000, &min_seen);
    TEST_ASSERT_EQUAL_INT32(-200, p.position());
    TEST_ASSERT_EQUAL_INT32(-200, min_seen);
}

void test_long_move_reaches_cruise_and_predicted_time(void)
{
    StepPlanner p;
    p.set_target(500);
    float peak = 0.0f;
    const int ticks = run_checked(p, 5000, nullptr, nullptr, &peak);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 400.0f, peak);
    // Trapezoid: 0.5 s ramp up + (500 - 200) / 400 s cruise + 0.5 s ramp down = 1.75 s; allow the creep tail.
    TEST_ASSERT_INT_WITHIN(150, 1750, ticks);
    TEST_ASSERT_EQUAL_INT32(500, p.position());
}

void test_short_move_is_triangular(void)
{
    StepPlanner p;
    p.set_target(40);
    float peak = 0.0f;
    run_checked(p, 5000, nullptr, nullptr, &peak);
    TEST_ASSERT_TRUE(peak < 300.0f); // sqrt(a * d) = 179 steps/s ideal, never reaches 400
    TEST_ASSERT_EQUAL_INT32(40, p.position());
}

void test_reversal_decelerates_then_arrives(void)
{
    StepPlanner p;
    p.set_target(400);
    while (p.speed() < 300.0f)
    {
        p.tick();
    }
    const int32_t at_switch = p.position();
    p.set_target(-100);
    int32_t max_seen = at_switch;
    int32_t min_seen = at_switch;
    run_checked(p, 10000, &min_seen, &max_seen);
    TEST_ASSERT_TRUE(max_seen > at_switch); // kept moving forward while braking (no instant flip)
    TEST_ASSERT_EQUAL_INT32(-100, p.position());
    TEST_ASSERT_EQUAL_INT32(-100, min_seen);
}

void test_retarget_same_direction_extends_move(void)
{
    StepPlanner p;
    p.set_target(100);
    for (int i = 0; i < 300; ++i)
    {
        p.tick();
    }
    p.set_target(250);
    run_checked(p, 5000);
    TEST_ASSERT_EQUAL_INT32(250, p.position());
}

void test_target_is_clamped_to_limits(void)
{
    StepPlanner p;
    p.set_target(10000);
    TEST_ASSERT_EQUAL_INT32(512, p.target());
    run_checked(p, 10000);
    TEST_ASSERT_EQUAL_INT32(512, p.position());
}

void test_jog_ramps_up_and_down(void)
{
    StepPlanner p;
    p.set_velocity(300.0f);
    for (int i = 0; i < 100; ++i)
    {
        p.tick();
    }
    TEST_ASSERT_FLOAT_WITHIN(25.0f, 80.0f, p.speed()); // 0.1 s at 800 steps/s^2 (plus the creep start)
    for (int i = 0; i < 400; ++i)
    {
        p.tick();
    }
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 300.0f, p.speed());
    const int32_t moved = p.position();
    TEST_ASSERT_TRUE(moved > 0);
    p.set_velocity(0.0f);
    run_checked(p, 2000);
    TEST_ASSERT_TRUE(p.is_idle());
    TEST_ASSERT_TRUE(p.position() >= moved);
}

void test_jog_stops_at_limit(void)
{
    StepPlanner p;
    p.set_velocity(400.0f);
    int32_t max_seen = 0;
    for (int i = 0; i < 5000; ++i)
    {
        p.tick();
        if (p.position() > max_seen)
            max_seen = p.position();
    }
    TEST_ASSERT_TRUE(max_seen <= 512);
    TEST_ASSERT_TRUE(p.position() >= 500); // brakes into the limit, does not stop far short
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, p.speed());
}

void test_reset_position(void)
{
    StepPlanner p;
    p.set_target(50);
    run_checked(p, 5000);
    p.reset_position(0);
    TEST_ASSERT_EQUAL_INT32(0, p.position());
    TEST_ASSERT_EQUAL_INT32(0, p.target());
    TEST_ASSERT_TRUE(p.is_idle());
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_zero_distance_stays_idle);
    RUN_TEST(test_reaches_target_exactly_without_overshoot);
    RUN_TEST(test_negative_target);
    RUN_TEST(test_long_move_reaches_cruise_and_predicted_time);
    RUN_TEST(test_short_move_is_triangular);
    RUN_TEST(test_reversal_decelerates_then_arrives);
    RUN_TEST(test_retarget_same_direction_extends_move);
    RUN_TEST(test_target_is_clamped_to_limits);
    RUN_TEST(test_jog_ramps_up_and_down);
    RUN_TEST(test_jog_stops_at_limit);
    RUN_TEST(test_reset_position);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to confirm it fails.** `pio test -e native -f native/test_step_planner` should fail because `step_planner.h` is not found.

- [ ] **Step 3: Create `step_planner.h`**

```cpp
/**
 * @file step_planner.h
 * @brief Trapezoidal velocity planner for the pan stepper, ticked at 1 kHz (pure C++17, natively tested).
 * @details Target mode moves to an absolute step count: accelerate, cruise, brake so it stops exactly on the target
 * (no overshoot); a new target mid-move is reached by braking first, never by an instant reversal. Jog mode follows a
 * commanded velocity with the same acceleration limit. Both modes brake into the soft limits and never step past
 * them. tick() returns the step to emit this tick (-1, 0, +1); at most one per tick (max 400 steps/s < 1 kHz).
 * NOT thread-safe: the motor task owns it.
 */

#pragma once

#include <cstdint>

namespace aiming
{
struct PlannerConfig {
    float max_speed = 400.0f; ///< steps/s
    float accel = 800.0f;     ///< steps/s^2
    float min_speed = 20.0f;  ///< creep speed: start/finish at this without ramping
    int32_t min_pos = -512;   ///< soft limits (+-90 deg at 2048 steps/rev)
    int32_t max_pos = 512;
    float tick_s = 0.001f;
};

class StepPlanner
{
public:
    explicit StepPlanner(const PlannerConfig& config = PlannerConfig{});

    void set_target(int32_t steps); ///< absolute, clamped to the soft limits; switches to target mode
    void set_velocity(float steps_per_s); ///< jog; clamped to +-max_speed; switches to jog mode
    int tick();

    int32_t position() const { return position_; }
    int32_t target() const { return target_; }
    float speed() const { return speed_; }
    bool is_idle() const;
    void reset_position(int32_t steps = 0); ///< redefine the current position (zero_here); stops motion
    const PlannerConfig& config() const { return config_; }

private:
    enum class Mode : uint8_t {
        Target,
        Jog
    };

    float goal_speed();

    PlannerConfig config_;
    Mode mode_ = Mode::Target;
    int32_t position_ = 0;
    int32_t target_ = 0;
    float jog_speed_ = 0.0f;
    float speed_ = 0.0f;
    float frac_ = 0.0f;
};
} // namespace aiming
```

- [ ] **Step 4: Create `step_planner.cpp`**

```cpp
#include "step_planner.h"

#include <algorithm>
#include <cmath>

namespace aiming
{
namespace
{
int sign(float v) { return (v > 0.0f) - (v < 0.0f); }
} // namespace

StepPlanner::StepPlanner(const PlannerConfig& config) : config_(config) {}

void StepPlanner::set_target(int32_t steps)
{
    mode_ = Mode::Target;
    target_ = std::clamp(steps, config_.min_pos, config_.max_pos);
}

void StepPlanner::set_velocity(float steps_per_s)
{
    mode_ = Mode::Jog;
    jog_speed_ = std::clamp(steps_per_s, -config_.max_speed, config_.max_speed);
}

bool StepPlanner::is_idle() const
{
    if (speed_ != 0.0f)
    {
        return false;
    }
    return mode_ == Mode::Target ? position_ == target_ : jog_speed_ == 0.0f;
}

void StepPlanner::reset_position(int32_t steps)
{
    position_ = steps;
    target_ = steps;
    mode_ = Mode::Target;
    jog_speed_ = 0.0f;
    speed_ = 0.0f;
    frac_ = 0.0f;
}

float StepPlanner::goal_speed()
{
    const float brake_dist = speed_ * speed_ / (2.0f * config_.accel);
    if (mode_ == Mode::Jog)
    {
        const int dir = sign(jog_speed_);
        if (dir == 0)
        {
            return 0.0f;
        }
        const int32_t room = dir > 0 ? config_.max_pos - position_ : position_ - config_.min_pos;
        if (static_cast<float>(room) <= brake_dist + 1.0f)
        {
            return 0.0f; // brake into the soft limit
        }
        return jog_speed_;
    }

    const int32_t r = target_ - position_;
    const int dir = r != 0 ? (r > 0 ? 1 : -1) : -sign(speed_);
    if (speed_ * static_cast<float>(dir) < 0.0f)
    {
        return 0.0f; // moving away from the target: brake first
    }
    if (static_cast<float>(std::abs(r)) <= brake_dist + 1.0f)
    {
        return 0.0f; // close enough that we must brake now
    }
    return static_cast<float>(dir) * config_.max_speed;
}

int StepPlanner::tick()
{
    const int32_t r = target_ - position_;
    if (mode_ == Mode::Target && r == 0 && std::fabs(speed_) <= config_.min_speed)
    {
        speed_ = 0.0f;
        frac_ = 0.0f;
        return 0;
    }

    const float dv = config_.accel * config_.tick_s;
    const float goal = goal_speed();
    if (speed_ < goal)
    {
        speed_ = std::min(speed_ + dv, goal);
    } else if (speed_ > goal)
    {
        speed_ = std::max(speed_ - dv, goal);
    }

    if (mode_ == Mode::Target && r != 0)
    {
        const float dir = r > 0 ? 1.0f : -1.0f;
        // Creep at min_speed toward the target: start without ramping, and never stall short of it while braking.
        if (speed_ * dir >= 0.0f && std::fabs(speed_) < config_.min_speed)
        {
            speed_ = dir * config_.min_speed;
        }
    } else if (mode_ == Mode::Jog && goal != 0.0f && std::fabs(speed_) < config_.min_speed &&
               speed_ * goal >= 0.0f)
    {
        speed_ = static_cast<float>(sign(goal)) * config_.min_speed;
    } else if (mode_ == Mode::Jog && goal == 0.0f && std::fabs(speed_) < config_.min_speed)
    {
        speed_ = 0.0f; // finished braking
        frac_ = 0.0f;
    }

    frac_ += speed_ * config_.tick_s;
    int step = 0;
    if (frac_ >= 1.0f)
    {
        step = 1;
        frac_ -= 1.0f;
    } else if (frac_ <= -1.0f)
    {
        step = -1;
        frac_ += 1.0f;
    }
    if (step != 0)
    {
        const int32_t next = position_ + step;
        if (next < config_.min_pos || next > config_.max_pos)
        {
            speed_ = 0.0f; // hard stop at a soft limit (only reachable if braking was cut short)
            frac_ = 0.0f;
            return 0;
        }
        position_ = next;
    }
    return step;
}
} // namespace aiming
```

- [ ] **Step 5: Run until it passes.** `pio test -e native -f native/test_step_planner` should report 11 tests, 0 failures, then the full native suite should pass.
  - Do not loosen the test limits.
  - If a test fails, debug the planner logic. This is the subtle part of the stage.
  - If the trapezoid-time tolerance turns out wrong, report the measured tick count and why. Do not just widen it.

- [ ] **Step 6: Commit**

```bash
git add ESP32/lib/aiming ESP32/test/native/test_step_planner
git commit -m "Add trapezoidal 1 kHz step planner with target and jog modes

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: AimPolicy, ButtonClassifier, CalibrationSession

**Files:**
- Create: `ESP32/lib/aiming/include/aim_policy.h`, `src/aim_policy.cpp`
- Create: `ESP32/lib/aiming/include/button_classifier.h`, `src/button_classifier.cpp`
- Create: `ESP32/lib/aiming/include/calibration_session.h`, `src/calibration_session.cpp`
- Test: `ESP32/test/native/test_aim_policy/test_aim_policy.cpp`

**Interfaces:**
- **Consumes:**
  - Task 1: `AimMap`, `Angles`, `CalPoint`, `fit`, `default_map`, `kTiltHomeDeg`, `pan_deg_to_steps`.
  - Task 2: `StepPlanner`.
  - `tracking::TrackPhase` from `tracking_types.h`.
- **Produces:**
  - `struct aiming::TrackView {tracking::TrackPhase state; float x; float y;}`
  - `enum class aiming::AimKind {Hold, Aim, Home}`, `struct aiming::AimCommand {AimKind kind; Angles angles;}`
  - `struct aiming::PolicyConfig {float deadband_deg = 1.0f; uint32_t home_delay_ms = 2000;}`
  - `class aiming::AimPolicy { AimPolicy(const AimMap&, const PolicyConfig& = {}); void set_map(const AimMap&); AimCommand update(const TrackView&, uint32_t now_ms); void reset(); }`
  - `enum class aiming::ButtonEvent {None, Short, Long}`, `struct aiming::ButtonConfig {uint32_t debounce_ms = 50; uint32_t long_ms = 1500;}`
  - `class aiming::ButtonClassifier { explicit ButtonClassifier(const ButtonConfig& = {}); ButtonEvent update(bool pressed_raw, uint32_t now_ms); }`
  - `enum class aiming::CalState {Idle, WaitPoint1, WaitPoint2, Done, Failed}`
  - `class aiming::CalibrationSession { void start(); void cancel(); CalState record(float x, float y, Angles at, bool target_visible); CalState state() const; int points() const; std::optional<AimMap> result() const; const char* failure() const; }`

- [ ] **Step 1: Write the failing tests** `ESP32/test/native/test_aim_policy/test_aim_policy.cpp`

```cpp
/**
 * @file test_aim_policy.cpp
 * @brief Host-side tests for the aim policy, the button classifier, the calibration session, and a
 * policy-to-planner pipeline check.
 */

#include <unity.h>

#include "aim_policy.h"
#include "button_classifier.h"
#include "calibration_session.h"
#include "step_planner.h"

#include <cmath>

using namespace aiming;
using tracking::TrackPhase;

void setUp(void) {}
void tearDown(void) {}

// ---- AimPolicy ----

void test_locked_aims_at_mapped_angles(void)
{
    AimPolicy p(default_map());
    const AimCommand c = p.update({TrackPhase::Locked, 320.0f, 120.0f}, 0);
    TEST_ASSERT_TRUE(c.kind == AimKind::Aim);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 33.0f, c.angles.pan);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, c.angles.tilt);
}

void test_deadband_suppresses_small_changes(void)
{
    AimPolicy p(default_map());
    p.update({TrackPhase::Locked, 160.0f, 120.0f}, 0);
    // 4 px = 0.825 deg pan: inside the 1 deg deadband
    TEST_ASSERT_TRUE(p.update({TrackPhase::Locked, 164.0f, 120.0f}, 60).kind == AimKind::Hold);
    // 6 px = 1.24 deg: outside
    const AimCommand c = p.update({TrackPhase::Locked, 166.0f, 120.0f}, 120);
    TEST_ASSERT_TRUE(c.kind == AimKind::Aim);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.2375f, c.angles.pan);
}

void test_coasting_aims_and_tentative_holds(void)
{
    AimPolicy p(default_map());
    TEST_ASSERT_TRUE(p.update({TrackPhase::Tentative, 300.0f, 120.0f}, 0).kind == AimKind::Hold);
    TEST_ASSERT_TRUE(p.update({TrackPhase::Coasting, 300.0f, 120.0f}, 60).kind == AimKind::Aim);
}

void test_idle_before_any_target_holds(void)
{
    AimPolicy p(default_map());
    TEST_ASSERT_TRUE(p.update({TrackPhase::Idle, 0.0f, 0.0f}, 10000).kind == AimKind::Hold);
}

void test_idle_holds_then_homes_once(void)
{
    AimPolicy p(default_map());
    p.update({TrackPhase::Locked, 300.0f, 60.0f}, 1000);
    TEST_ASSERT_TRUE(p.update({TrackPhase::Idle, 0, 0}, 2999).kind == AimKind::Hold);
    const AimCommand home = p.update({TrackPhase::Idle, 0, 0}, 3000); // exactly 2000 ms after last seen
    TEST_ASSERT_TRUE(home.kind == AimKind::Home);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, home.angles.pan);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, kTiltHomeDeg, home.angles.tilt);
    TEST_ASSERT_TRUE(p.update({TrackPhase::Idle, 0, 0}, 5000).kind == AimKind::Hold); // only once
}

void test_relock_after_home_aims(void)
{
    AimPolicy p(default_map());
    p.update({TrackPhase::Locked, 300.0f, 60.0f}, 0);
    p.update({TrackPhase::Idle, 0, 0}, 2500);
    // Same spot as before homing: must aim again even though it equals the old setpoint.
    TEST_ASSERT_TRUE(p.update({TrackPhase::Locked, 300.0f, 60.0f}, 3000).kind == AimKind::Aim);
}

void test_millis_wraparound(void)
{
    AimPolicy p(default_map());
    p.update({TrackPhase::Locked, 300.0f, 60.0f}, 0xFFFFFF00u);
    TEST_ASSERT_TRUE(p.update({TrackPhase::Idle, 0, 0}, 0x00000100u).kind == AimKind::Hold);  // 512 ms later
    TEST_ASSERT_TRUE(p.update({TrackPhase::Idle, 0, 0}, 0x000007D0u).kind == AimKind::Home); // 2256 ms later
}

void test_reset_forgets_setpoint(void)
{
    AimPolicy p(default_map());
    p.update({TrackPhase::Locked, 300.0f, 60.0f}, 0);
    p.reset();
    TEST_ASSERT_TRUE(p.update({TrackPhase::Locked, 300.0f, 60.0f}, 10).kind == AimKind::Aim);
    TEST_ASSERT_TRUE(p.update({TrackPhase::Idle, 0, 0}, 20).kind == AimKind::Hold);
}

// ---- ButtonClassifier ----

/** Feed a constant raw level for [from, to) in 1 ms ticks; returns the last non-None event. */
static ButtonEvent feed(ButtonClassifier& b, bool pressed, uint32_t from, uint32_t to, int* count = nullptr)
{
    ButtonEvent last = ButtonEvent::None;
    for (uint32_t t = from; t < to; ++t)
    {
        const ButtonEvent e = b.update(pressed, t);
        if (e != ButtonEvent::None)
        {
            last = e;
            if (count)
                ++*count;
        }
    }
    return last;
}

void test_short_press(void)
{
    ButtonClassifier b;
    TEST_ASSERT_TRUE(feed(b, true, 0, 300) == ButtonEvent::None);
    TEST_ASSERT_TRUE(feed(b, false, 300, 400) == ButtonEvent::Short);
}

void test_long_press_fires_once_at_threshold(void)
{
    ButtonClassifier b;
    // The press is confirmed at t = 50 (debounce); long fires 1500 ms after that.
    TEST_ASSERT_TRUE(feed(b, true, 0, 1549) == ButtonEvent::None);
    int count = 0;
    TEST_ASSERT_TRUE(feed(b, true, 1549, 3000, &count) == ButtonEvent::Long);
    TEST_ASSERT_EQUAL_INT(1, count);
    TEST_ASSERT_TRUE(feed(b, false, 3000, 3100) == ButtonEvent::None); // release after long: no short
}

void test_bounce_is_ignored(void)
{
    ButtonClassifier b;
    feed(b, true, 0, 20);  // 20 ms glitch < 50 ms debounce
    TEST_ASSERT_TRUE(feed(b, false, 20, 500) == ButtonEvent::None);
}

// ---- CalibrationSession ----

void test_calibration_happy_path(void)
{
    CalibrationSession s;
    TEST_ASSERT_TRUE(s.state() == CalState::Idle);
    s.start();
    TEST_ASSERT_TRUE(s.state() == CalState::WaitPoint1);
    TEST_ASSERT_TRUE(s.record(40, 50, Angles{-30, 110}, true) == CalState::WaitPoint2);
    TEST_ASSERT_EQUAL_INT(1, s.points());
    TEST_ASSERT_TRUE(s.record(280, 200, Angles{25, 70}, true) == CalState::Done);
    TEST_ASSERT_EQUAL_INT(2, s.points());
    TEST_ASSERT_TRUE(s.result().has_value());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, s.result()->aim(280, 200).pan);
}

void test_record_without_target_is_ignored(void)
{
    CalibrationSession s;
    s.start();
    TEST_ASSERT_TRUE(s.record(40, 50, Angles{-30, 110}, false) == CalState::WaitPoint1);
    TEST_ASSERT_EQUAL_INT(0, s.points());
}

void test_bad_fit_fails_and_next_press_restarts(void)
{
    CalibrationSession s;
    s.start();
    s.record(100, 100, Angles{0, 90}, true);
    TEST_ASSERT_TRUE(s.record(120, 110, Angles{5, 85}, true) == CalState::Failed);
    TEST_ASSERT_NOT_NULL(s.failure());
    TEST_ASSERT_FALSE(s.result().has_value());
    // In Failed, the next press restarts at point 1 without recording.
    TEST_ASSERT_TRUE(s.record(40, 50, Angles{-30, 110}, true) == CalState::WaitPoint1);
    TEST_ASSERT_EQUAL_INT(0, s.points());
}

void test_cancel_from_any_state(void)
{
    CalibrationSession s;
    s.start();
    s.record(40, 50, Angles{-30, 110}, true);
    s.cancel();
    TEST_ASSERT_TRUE(s.state() == CalState::Idle);
    TEST_ASSERT_EQUAL_INT(0, s.points());
    TEST_ASSERT_TRUE(s.record(40, 50, Angles{-30, 110}, true) == CalState::Idle); // ignored when Idle
}

// ---- Pipeline: policy -> planner ----

void test_policy_to_planner_reaches_aim(void)
{
    AimPolicy p(default_map());
    StepPlanner planner;
    const AimCommand c = p.update({TrackPhase::Locked, 280.0f, 120.0f}, 0); // pan 24.75 deg
    TEST_ASSERT_TRUE(c.kind == AimKind::Aim);
    planner.set_target(pan_deg_to_steps(c.angles.pan));
    int ticks = 0;
    while (!planner.is_idle() && ticks < 5000)
    {
        planner.tick();
        ++ticks;
    }
    TEST_ASSERT_EQUAL_INT32(141, planner.position()); // 24.75 * 2048 / 360 = 140.8
    TEST_ASSERT_TRUE(ticks < 1100); // triangular move of 141 steps: ~0.84 s plus creep
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_locked_aims_at_mapped_angles);
    RUN_TEST(test_deadband_suppresses_small_changes);
    RUN_TEST(test_coasting_aims_and_tentative_holds);
    RUN_TEST(test_idle_before_any_target_holds);
    RUN_TEST(test_idle_holds_then_homes_once);
    RUN_TEST(test_relock_after_home_aims);
    RUN_TEST(test_millis_wraparound);
    RUN_TEST(test_reset_forgets_setpoint);
    RUN_TEST(test_short_press);
    RUN_TEST(test_long_press_fires_once_at_threshold);
    RUN_TEST(test_bounce_is_ignored);
    RUN_TEST(test_calibration_happy_path);
    RUN_TEST(test_record_without_target_is_ignored);
    RUN_TEST(test_bad_fit_fails_and_next_press_restarts);
    RUN_TEST(test_cancel_from_any_state);
    RUN_TEST(test_policy_to_planner_reaches_aim);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to confirm it fails.** The headers are not found.

- [ ] **Step 3: Create `aim_policy.h` / `.cpp`**

```cpp
/**
 * @file aim_policy.h
 * @brief Decides where the turret should point from the tracker state (pure C++17, natively tested).
 * @details Locked/Coasting: aim at the mapped target position, changing the setpoint only when the new aim is more
 * than deadband_deg away on either axis (no jitter). Tentative: hold. Idle: hold, and home_delay_ms after the target
 * was last seen, return Home exactly once. Time is millis()-style uint32 and wraparound-safe.
 */

#pragma once

#include "aim_map.h"
#include "tracking_types.h"

#include <cstdint>

namespace aiming
{
struct TrackView {
    tracking::TrackPhase state = tracking::TrackPhase::Idle;
    float x = 0.0f; ///< target centroid, frame pixels
    float y = 0.0f;
};

enum class AimKind : uint8_t {
    Hold,
    Aim,
    Home
};

struct AimCommand {
    AimKind kind = AimKind::Hold;
    Angles angles;
};

struct PolicyConfig {
    float deadband_deg = 1.0f;
    uint32_t home_delay_ms = 2000;
};

class AimPolicy
{
public:
    explicit AimPolicy(const AimMap& map, const PolicyConfig& config = PolicyConfig{});

    void set_map(const AimMap& map) { map_ = map; }
    AimCommand update(const TrackView& track, uint32_t now_ms);
    void reset(); ///< forget the setpoint and the lost-target timer (mode changes)

private:
    AimMap map_;
    PolicyConfig config_;
    bool have_setpoint_ = false;
    Angles setpoint_;
    bool seen_ = false;
    bool homed_ = false;
    uint32_t last_seen_ms_ = 0;
};
} // namespace aiming
```

```cpp
#include "aim_policy.h"

#include <cmath>

namespace aiming
{
AimPolicy::AimPolicy(const AimMap& map, const PolicyConfig& config) : map_(map), config_(config) {}

void AimPolicy::reset()
{
    have_setpoint_ = false;
    seen_ = false;
    homed_ = false;
}

AimCommand AimPolicy::update(const TrackView& track, uint32_t now_ms)
{
    using tracking::TrackPhase;
    if (track.state == TrackPhase::Locked || track.state == TrackPhase::Coasting)
    {
        seen_ = true;
        homed_ = false;
        last_seen_ms_ = now_ms;
        const Angles a = map_.aim(track.x, track.y);
        if (!have_setpoint_ || std::fabs(a.pan - setpoint_.pan) > config_.deadband_deg ||
            std::fabs(a.tilt - setpoint_.tilt) > config_.deadband_deg)
        {
            setpoint_ = a;
            have_setpoint_ = true;
            return AimCommand{AimKind::Aim, a};
        }
        return AimCommand{AimKind::Hold, setpoint_};
    }
    if (track.state == TrackPhase::Idle && seen_ && !homed_ &&
        static_cast<uint32_t>(now_ms - last_seen_ms_) >= config_.home_delay_ms)
    {
        homed_ = true;
        have_setpoint_ = false;
        return AimCommand{AimKind::Home, Angles{0.0f, kTiltHomeDeg}};
    }
    return AimCommand{AimKind::Hold, setpoint_};
}
} // namespace aiming
```

- [ ] **Step 4: Create `button_classifier.h` / `.cpp`**

```cpp
/**
 * @file button_classifier.h
 * @brief Turns a raw push-button level into Short / Long press events (pure C++17, natively tested).
 * @details Debounces the raw level (it must be stable for debounce_ms). A press released before long_ms after it was
 * confirmed is Short (reported on release). A press still held long_ms after confirmation is Long (reported once,
 * at that moment; the release then reports nothing). Call update() every loop pass with millis().
 */

#pragma once

#include <cstdint>

namespace aiming
{
enum class ButtonEvent : uint8_t {
    None,
    Short,
    Long
};

struct ButtonConfig {
    uint32_t debounce_ms = 50;
    uint32_t long_ms = 1500;
};

class ButtonClassifier
{
public:
    explicit ButtonClassifier(const ButtonConfig& config = ButtonConfig{}) : config_(config) {}
    ButtonEvent update(bool pressed_raw, uint32_t now_ms);

private:
    ButtonConfig config_;
    bool raw_ = false;
    uint32_t raw_since_ms_ = 0;
    bool stable_ = false;
    uint32_t pressed_at_ms_ = 0;
    bool long_fired_ = false;
};
} // namespace aiming
```

```cpp
#include "button_classifier.h"

namespace aiming
{
ButtonEvent ButtonClassifier::update(bool pressed_raw, uint32_t now_ms)
{
    if (pressed_raw != raw_)
    {
        raw_ = pressed_raw;
        raw_since_ms_ = now_ms;
    }
    if (raw_ != stable_ && static_cast<uint32_t>(now_ms - raw_since_ms_) >= config_.debounce_ms)
    {
        stable_ = raw_;
        if (stable_)
        {
            pressed_at_ms_ = now_ms;
            long_fired_ = false;
            return ButtonEvent::None;
        }
        return long_fired_ ? ButtonEvent::None : ButtonEvent::Short;
    }
    if (stable_ && !long_fired_ && static_cast<uint32_t>(now_ms - pressed_at_ms_) >= config_.long_ms)
    {
        long_fired_ = true;
        return ButtonEvent::Long;
    }
    return ButtonEvent::None;
}
} // namespace aiming
```

Check `test_long_press_fires_once_at_threshold` against this code:
- Raw goes high at t = 0.
- The stable press is confirmed at t = 50, when `now - raw_since = 50 >= 50`.
- Long fires when `now - 50 >= 1500`, at t = 1550. That falls in `[1549, 3000)` and not in `[0, 1549)`, so the test's split is right.

- [ ] **Step 5: Create `calibration_session.h` / `.cpp`**

```cpp
/**
 * @file calibration_session.h
 * @brief Two-point joystick calibration as a state machine (pure C++17, natively tested).
 * @details start() -> WaitPoint1 -> WaitPoint2 -> Done (result() holds the fitted map) or Failed (failure() says
 * why). record() ignores presses while no target is visible (the point is meaningless) and while Idle; in Failed
 * the next record() restarts at point 1 without recording. cancel() returns to Idle from any state.
 */

#pragma once

#include "aim_map.h"

#include <optional>

namespace aiming
{
enum class CalState : uint8_t {
    Idle,
    WaitPoint1,
    WaitPoint2,
    Done,
    Failed
};

class CalibrationSession
{
public:
    void start();
    void cancel();
    CalState record(float x, float y, Angles at, bool target_visible);

    CalState state() const { return state_; }
    int points() const { return points_; }
    std::optional<AimMap> result() const { return result_; }
    const char* failure() const { return failure_; }

private:
    CalState state_ = CalState::Idle;
    int points_ = 0;
    CalPoint first_;
    std::optional<AimMap> result_;
    const char* failure_ = nullptr;
};
} // namespace aiming
```

```cpp
#include "calibration_session.h"

namespace aiming
{
void CalibrationSession::start()
{
    state_ = CalState::WaitPoint1;
    points_ = 0;
    result_.reset();
    failure_ = nullptr;
}

void CalibrationSession::cancel()
{
    state_ = CalState::Idle;
    points_ = 0;
    result_.reset();
    failure_ = nullptr;
}

CalState CalibrationSession::record(float x, float y, Angles at, bool target_visible)
{
    if (state_ == CalState::Failed)
    {
        start();
        return state_;
    }
    if ((state_ != CalState::WaitPoint1 && state_ != CalState::WaitPoint2) || !target_visible)
    {
        return state_;
    }
    const CalPoint p{x, y, at.pan, at.tilt};
    if (state_ == CalState::WaitPoint1)
    {
        first_ = p;
        points_ = 1;
        state_ = CalState::WaitPoint2;
        return state_;
    }
    points_ = 2;
    result_ = fit(first_, p);
    if (result_)
    {
        state_ = CalState::Done;
    } else
    {
        state_ = CalState::Failed;
        failure_ = "points too close (need 80 px apart in x and 60 px in y)";
    }
    return state_;
}
} // namespace aiming
```

- [ ] **Step 6: Run until it passes.** The new suite should report 16 tests, 0 failures, and the full native suite should pass. Then `pio run -e freenove_esp32_wrover`: SUCCESS.

- [ ] **Step 7: Commit**

```bash
git add ESP32/lib/aiming ESP32/test/native/test_aim_policy
git commit -m "Add aim policy, button classifier and calibration session

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Turret motor task

**Files:**
- Create: `ESP32/lib/motion/include/base_turret.h`, `ESP32/lib/motion/include/turret.h`, `ESP32/lib/motion/src/turret.cpp`
- Delete: `ESP32/lib/motion/include/movement_manager.h`, `ESP32/lib/motion/include/base_movement_manager.h`, `ESP32/lib/motion/src/movement_manager.cpp`
- Modify: `ESP32/platformio.ini` (remove `arduino-libraries/Stepper` from `lib_deps`)
- Modify: `ESP32/src/main.cpp`, `ESP32/lib/controller/include/controller.h`, `ESP32/lib/controller/src/controller.cpp`. Make only the minimal changes needed to compile against `BaseTurret` (USER mode jogs, AI mode does not aim yet). Task 5 adds the real mode logic.

**Interfaces:**
- **Consumes:** `aiming::StepPlanner`, `PlannerConfig`, `Angles`, `clamp_angles`, `pan_deg_to_steps`, `pan_steps_to_deg`, `kTiltHomeDeg`; `pins::STEPPER_IN1..IN4`, `pins::SERVO`; `ESP32Servo`.
- **Produces:**

```cpp
/**
 * @file base_turret.h
 * @brief What the controller can ask of the pan/tilt turret. Thread-safe: called from loop(), executed by the motor
 * task.
 */
#pragma once

#include "aim_map.h"

class BaseTurret
{
public:
    virtual ~BaseTurret() = default;
    /** @brief Move to absolute angles (clamped to the soft limits). */
    virtual void set_target_deg(const aiming::Angles& target) = 0;
    /** @brief Manual velocity, each axis -1..1 of its max speed; 0,0 stops (with deceleration). */
    virtual void jog(float pan, float tilt) = 0;
    /** @brief Same as set_target_deg({0, home tilt}). */
    virtual void home() = 0;
    /** @brief Current angles (pan from the step counter, tilt as last written to the servo). */
    virtual aiming::Angles position_deg() const = 0;
    /** @brief Current stepper speed, steps/s (signed). */
    virtual float pan_speed() const = 0;
};
```

**`Turret` (`turret.h` / `turret.cpp`) requirements.** The implementer writes this code; the brief gives the structure and exact values.

- `class Turret : public BaseTurret` with a constructor `explicit Turret(Servo& servo)` and `bool begin()`.
- **`begin()`:**
  - Sets the 4 coil pins as outputs and LOW.
  - Attaches the servo on `pins::SERVO` and writes `kTiltHomeDeg`.
  - Creates the motor task with `xTaskCreatePinnedToCore(task_entry, "motor", 3072, this, 3, &_task, 0)`.
  - Returns false if task creation fails.
  - It is idempotent.
- **Command mailbox.** Commands go through a `portMUX_TYPE`-protected struct with these fields:
  - kind: None, Target, Jog or Home
  - target angles
  - jog fractions
  - a sequence counter
  `set_target_deg`, `jog` and `home` write it under the lock and bump the sequence. `position_deg` and `pan_speed` read published values under the same lock. No blocking calls or logging happen inside the critical section.
- **Motor task loop (1 ms period, `vTaskDelayUntil`).**
  - **Commands.** When the command sequence changed, copy the command under the lock, then apply it:
    - Target: `planner.set_target(pan_deg_to_steps(clamped.pan))`; `tilt_target = clamped.tilt`; `tilt_rate = 0`.
    - Jog: `planner.set_velocity(jog_pan * max_speed)`; `tilt_rate = jog_tilt * 120` °/s.
    - Home: target `{0, kTiltHomeDeg}`.
  - **Stepping.** `const int step = planner.tick();`. If `step != 0`:
    - If the coils are off, first write the current phase so the coils energize.
    - Then `phase = (phase + step) & 3`, write the coils, and record the time of the last step.
    - Coil patterns for phases 0-3 on (IN1, IN2, IN3, IN4): `{1,1,0,0}`, `{0,1,1,0}`, `{0,0,1,1}`, `{1,0,0,1}`.
  - **Coil release.** If the coils are on, the planner is idle, and more than 500 ms have passed since the last step, write all 4 pins LOW.
  - **Servo (every 20th tick, 50 Hz).**
    - If `tilt_rate != 0`, `tilt_target += tilt_rate * 0.02`, clamped to [20, 160].
    - Move `tilt_current` toward `tilt_target` by at most `120 * 0.02 = 2.4°`.
    - If the rounded value changed, `servo.write(lround(tilt_current))`.
  - **Publishing.** Publish pan degrees (`pan_steps_to_deg(planner.position())`), tilt (`tilt_current`) and speed under the lock.
- Doxygen on the public header. The class comment explains why it is a task: blocking `Stepper::step()` would stall detection. It also explains why the coils are released: the 28BYJ-48 heats up when held energized.

**Minimal controller and main changes (compile only):**
- `Controller` takes `BaseTurret&` instead of `BaseMovementManager&`.
- USER mode calls `turret.jog(speed_x / 255.0f, speed_y / 255.0f)` at the existing 25 Hz. Joystick y positive means Up, which means a larger tilt angle.
- AI mode keeps running detection but does not aim (remove `kAiAimEnabled`; Task 5 adds aiming).
- `main.cpp`:
  - Remove the `Stepper` object and `#include "movement_manager.h"`.
  - Create `Turret turret(servo);`, pass it to `Controller`, and call `turret.begin()` in `setup()` in place of the old servo attach. Log an error if it fails.
  - Add ` pan=%.1f tilt=%.1f steps_s=%.0f` to the end of the stats format, from `turret.position_deg()` and `turret.pan_speed()`. Keep every existing field and its order.
- `grep -rn "Stepper\|movement_manager\|MovementManager\|BaseMovementManager" ESP32/src ESP32/lib ESP32/include` must return nothing afterwards. Stale on-device suites under `ESP32/test/` (not `test/native`) that reference them can be left alone; list them in the report.

- [ ] **Step 1:** Write `base_turret.h`, `turret.h` and `turret.cpp` per the requirements above.
- [ ] **Step 2:** Delete the old movement manager files and the `Stepper` lib_dep. Update `controller.h`/`.cpp` and `main.cpp` minimally.
- [ ] **Step 3: Verify.**
  - `pio run -e freenove_esp32_wrover > <log> 2>&1`: SUCCESS, no new warnings.
  - `pio run -e pin_check`: SUCCESS.
  - `pio check --severity=high --severity=medium`: no new findings in `lib/motion`, `lib/controller` or `src`.
  - `pio test -e native`: all PASSED.
  - clang-format 19 `--dry-run --Werror` on the touched files.
  - Report the RAM/Flash lines.
- [ ] **Step 4: Commit**

```bash
git add -A ESP32/lib/motion ESP32/lib/controller ESP32/src ESP32/platformio.ini
git commit -m "Drive the turret from a non-blocking motor task with an own coil driver

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Controller modes, aiming, calibration, NVS, /commands, HUD

**Files:**
- Modify: `ESP32/include/system_control_types.h`. Replace `SystemControl` with `enum class ControlMode : uint8_t {Ai, User, Calibrate}` and `const char* to_string(ControlMode)`, which returns `"ai"`, `"user"` or `"calibrate"`. Update any other users of `SystemControl` (grep).
- Create: `ESP32/lib/controller/include/cal_store.h`, `ESP32/lib/controller/src/cal_store.cpp`. These hold `bool cal_store::load(aiming::AimMap& out)` and `bool cal_store::save(const aiming::AimMap&)`, using Arduino `Preferences`. Namespace `"aim"`, key `"map"`, `putBytes`/`getBytes` of `aiming::serialize` / `deserialize`; a read-only open for load.
- Create: `ESP32/lib/controller/include/control_status.h`, containing:
  - `struct ControlStatus {ControlMode mode; uint8_t cal_points; aiming::CalState cal_state; bool calibrated; aiming::Angles angles; const char* cal_failure;}`
  - `class BaseStatusProvider { public: virtual ~BaseStatusProvider() = default; virtual ControlStatus control_status() const = 0; };`
- Modify: `ESP32/lib/controller/include/controller.h`, `src/controller.cpp`
- Modify: `ESP32/lib/network/include/turret_server.h`, `src/turret_server.cpp`, `include/websockets.h`
- Modify: `ESP32/src/main.cpp`, `web/web_ui.html`, `CLAUDE.md`

**Interfaces:**
- **Consumes:**
  - Tasks 1-3: `aiming::*`.
  - Task 4: `BaseTurret`.
  - Stage 2: `BaseDetectionModule::get_track_snapshot()`, which returns `TrackSnapshot{state, x, y, ...}` in pixels, with `detected()` true for Locked or Coasting.
  - `BaseDetectionModule::reset()`.

**Controller behaviour (exact).** `Controller` implements `BaseStatusProvider`. Its members are:
- `ButtonClassifier _button`
- `AimPolicy _policy`, constructed with `default_map()`
- `CalibrationSession _session`
- `ControlMode _mode = Ai`, `_mode_before_cal`
- `bool _calibrated`
- a published `ControlStatus` under a `portMUX`

It adds `void begin()`, called from `setup()` after the turret starts: if `cal_store::load(map)` succeeds, then `_policy.set_map(map)` and `_calibrated = true`.

`run()` handles one pass of the loop as follows.

**1. Button events.** Read `const uint32_t now = millis(); const ButtonEvent ev = _button.update(_joystick.is_z_held(), now);`. This replaces `is_z_pressed()`. `is_z_held()` is raw, and the classifier debounces.

**2. Long press.**
- **In Calibrate:** `_session.cancel()`, `_mode = _mode_before_cal`, `_turret.jog(0, 0)`.
- **Otherwise:** `_mode_before_cal = _mode`, `_mode = Calibrate`, `_session.start()`, `_policy.reset()`, `_turret.jog(0, 0)`.

Log the transition in both cases.

**3. Short press.**
- **In Calibrate:**
  - Read the snapshot `s = _detection.get_track_snapshot()`.
  - `const CalState st = _session.record(s.x, s.y, _turret.position_deg(), s.detected());`
  - If `st == Done`:
    - Save the map: `cal_store::save(*_session.result())`, logging an error if the save fails.
    - Then `_policy.set_map(*_session.result())`, `_calibrated = true`, `_session.cancel()`, `_mode = Ai`, `_policy.reset()`.
  - Log the state.
- **Otherwise:**
  - Toggle Ai and User.
  - Then `_detection.reset()`, `_policy.reset()`, `_turret.jog(0, 0)`.

**4. Per mode.**
- **User or Calibrate:** jog at 25 Hz, as in Task 4.
- **Calibrate also:** run detection once per new frame sequence (same lease code), so the HUD box shows. No policy.
- **Ai:**
  - Run detection once per new frame sequence.
  - Then `s = get_track_snapshot()` and `cmd = _policy.update({s.state, (float)s.x, (float)s.y}, now)`.
  - Act on it: Aim → `_turret.set_target_deg(cmd.angles)`; Home → `_turret.home()`; Hold → nothing.

**5. Publish.** Publish `ControlStatus` under the lock:
- mode
- `_session.points()`, `_session.state()`
- `_calibrated`
- `_turret.position_deg()`
- `_session.failure()`

**/commands JSON.**
- `HttpServer::start(FrameStore&, BaseDetectionModule*, const BaseStatusProvider* status = nullptr)` stores the status provider like the detection instance.
- Raise the JSON buffer from 256 to 384 bytes (malloc size and both snprintf bounds).
- After `"vy"`, append:
  `,"mode":"%s","cal":%u,"cal_state":"%s","calibrated":%d,"pan":%.1f,"tilt":%.1f,"cal_error":"%s"`
- `cal_state` is one of `idle|point1|point2|done|failed`.
- `cal_error` is `""` when the failure is null.
- When no provider is given, omit these fields.

**HUD (`web/web_ui.html`).**
- Show `MODE: AI|USER|CALIBRATE`, `PAN/TILT: x.x / y.y` and `UNCALIBRATED` when `calibrated` is 0.
- In calibrate mode, show a prompt banner:
  - point1: "Wave at a spot near one corner, aim the turret at it, press"
  - point2: "Now the opposite corner, aim, press"
  - failed: the `cal_error` text plus "press to restart"
- The prompt must also say "hold the button 1.5 s to cancel".
- Keep existing behaviour. Run the `node --check` syntax check on the inline script as in Stage 2.

**Stats line.** Append ` mode=%s` (`to_string(mode)`) before the pan field added in Task 4.

**CLAUDE.md.**
- Architecture: describe `lib/aiming`, the motor task, the modes and gestures (short/long press), calibration and NVS. Note that the camera is fixed and aiming is absolute.
- Replace the Movement bullet.
- Remove the "AI mode does not move the turret" sentence.
- Document the new stats fields.

- [ ] **Step 1:** Make the mode type, `cal_store`, `control_status` and controller changes.
- [ ] **Step 2:** Update the `/commands` JSON and `HttpServer::start`, and wire `main.cpp`: `http_server.start(frame_store, &detection_manager, &controller)` and `controller.begin()`.
- [ ] **Step 3:** HUD changes.
- [ ] **Step 4: Verify.**
  - Firmware build and pin_check: SUCCESS.
  - `pio check`: no new findings.
  - Native suite: all PASSED.
  - Web unittest: OK.
  - `node --check` on the HUD script.
  - clang-format clean.
  - No em dashes.
- [ ] **Step 5: Commit** in one or more commits with descriptive messages and the trailer.

---

### Task 6 (controller + user, hardware): verify on the board

Not dispatched: it needs the board and the user.

- [ ] Flash, read the IP and the stats line (port COM3 or COM4).
- [ ] **Manual mode.** The joystick jogs smoothly in both axes, and the soft limits hold. After about 0.5 s still, the coils are released (motor cool, `steps_s=0`).
- [ ] **Uncalibrated AI.** The turret roughly follows a hand using the default map, and the HUD shows UNCALIBRATED.
- [ ] **Calibration:**
  - A long press enters calibrate mode.
  - Record two points by waving at the spot, jogging until aimed, then pressing.
  - The mode returns to AI, and the HUD shows calibrated.
  - Reboot, and confirm the calibration is still applied.
- [ ] **Accuracy.** Check that a stationary target at 1-3 m is within about 5°.
  - A pointer or laser on the turret helps if available.
  - Watch that `decode_us_avg + track_us_avg` stays ≤ 15 ms and capture ≥ 16 fps while the turret moves.
- [ ] **Lost target.** The turret holds about 2 s, then homes.
- [ ] Push, open the PR with the benchmark table, and confirm CI is green.
