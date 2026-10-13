/**
 * @file cal_store.h
 * @brief Persists the aim calibration (an aiming::AimMap) in NVS.
 */

#pragma once

#include "aim_map.h"

namespace cal_store
{
/**
 * @brief Loads the saved calibration from NVS (namespace "aim", key "map").
 * @param out Receives the map when the stored blob is present, the right size and passes its CRC.
 * @return true if a valid map was loaded; false leaves @p out untouched (caller keeps the default map).
 */
bool load(aiming::AimMap& out);

/**
 * @brief Saves the calibration to NVS (namespace "aim", key "map").
 * @return true if the whole blob was written.
 */
bool save(const aiming::AimMap& map);
} // namespace cal_store
