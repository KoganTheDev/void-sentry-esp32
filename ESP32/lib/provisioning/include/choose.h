/**
 * @file choose.h
 * @brief Picks which saved networks to try, given a scan (pure C++20, no heap).
 */

#pragma once

#include "network_list.h"

#include <span>

namespace prov
{
/**
 * @brief Saved networks present in the scan (byte-exact SSID), strongest RSSI first.
 * @details Several APs with one SSID count by their strongest RSSI. Ties keep list order. Scan entries with an
 * empty SSID are ignored.
 */
Candidates choose_networks(std::span<const ScanResult> scan, const NetworkList& list);
} // namespace prov
