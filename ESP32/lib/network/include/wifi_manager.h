/**
 * @file WifiManager.h
 * @brief Static utility for managing non-blocking ESP32 WiFi connectivity.
 */

#pragma once
#include <WiFi.h>

/**
 * @class WifiManager
 * @brief Handles asynchronous WiFi connection and automated reconnection logic.
 */
class WifiManager
{
public:
    /**
     * @brief Initiates an asynchronous connection to a WiFi access point.
     * @details Sets mode to WIFI_STA and disables sleep modes for better hardware
     * stability.
     * @param ssid Service Set Identifier of the network.
     * @param password key for the network.
     */
    static void connect(const String& ssid, const String& password);

    /**
     * @brief Polls the WiFi hardware for current connection status.
     * @return true if currently connected to an AP with a valid IP.
     */
    static bool is_connected();

    /**
     * @brief Returns the local IPv4 address as a string.
     * @return String formatted IP (e.g. "192.168.1.50") or "0.0.0.0" if
     * unavailable.
     */
    static String get_ip();

    /**
     * @brief Heartbeat function. Checks if WiFi has diconnected, if so, attempts
     * to reconnect.
     * @details Monitors connection health. If the connection is lost, it manages
     * throttle-controlled reconnection attempts to avoid CPU spikes.
     */
    static void maintain();
};