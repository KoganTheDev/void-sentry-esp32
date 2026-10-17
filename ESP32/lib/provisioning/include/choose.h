/**
 * @file choose.h
 * @brief Picks which saved networks to try, given a scan (pure C++17, no heap).
 */

#pragma once

#include "network_list.h"

namespace prov
{
/**
 * @brief Saved networks present in the scan (byte-exact SSID), strongest RSSI first.
 * @details Several APs with one SSID count by their strongest RSSI. Ties keep list order. Scan entries with an
 * empty SSID are ignored.
 */
Candidates choose_networks(const ScanResult* scan, size_t scan_count, const NetworkList& list);
} // namespace prov
