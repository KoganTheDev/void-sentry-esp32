/**
 * @file wifi_store.h
 * @brief NVS persistence for the saved WiFi networks, the PIN record and the setup hotspot password.
 */

#pragma once

#include "blobs.h"
#include <Arduino.h>

/**
 * @class WifiStore
 * @brief Reads and writes the `wifi` NVS namespace (keys `nets`, `pin`, `appw`) using the pure blob formats.
 * @details Every call opens and closes the namespace, so the object holds no state. Not thread-safe by itself:
 * WifiManager calls it under its mutex.
 */
class WifiStore
{
public:
    /**
     * @brief Loads the saved networks and the PIN record.
     * @details A missing, truncated or corrupt blob is treated as empty (one log line, no crash).
     * @param[out] list Receives the saved networks (cleared first).
     * @param[out] pin Receives the PIN record (default-constructed, not set, when absent).
     * @return true if a valid network list was loaded.
     */
    bool load(prov::NetworkList& list, prov::PinRecord& pin);

    /** @brief Stores the network list blob. @return true if all bytes were written. */
    bool save_nets(const prov::NetworkList& list);

    /** @brief Stores the PIN record blob. @return true if all bytes were written. */
    bool save_pin(const prov::PinRecord& rec);

    /**
     * @brief Returns the setup hotspot password, creating and storing 10 random characters on the first call.
     * @details Characters come from an alphabet without 0/O/1/l/I, drawn with esp_random().
     */
    String hotspot_password();

    /** @brief Erases the saved networks and the PIN (the hotspot password is kept). */
    void erase_all();
};
