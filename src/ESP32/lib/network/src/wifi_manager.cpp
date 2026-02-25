#include "wifi_manager.h"
#include <esp_log.h>

const char* TAG = "WIFI_NAMAGER";

void WifiManager::connect(const String& ssid, const String& password)
{
    if (ssid.length() == 0)
    {
        ESP_LOGE(TAG, "SSID is empty");
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    WiFi.setAutoReconnect(true);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);

        if (millis() > 30000)
        { // 30 second timeout
            ESP_LOGE(TAG, "Connection Failed: Timeout");
            return;
        }
    }

    Serial.printf("\n[WIFI] Connected! ESP's IP Address: %s\n", get_ip().c_str());
}

bool WifiManager::is_connected() { return WiFi.status() == WL_CONNECTED; }

String WifiManager::get_ip() { return WiFi.localIP().toString(); }

void WifiManager::maintain()
{
    static unsigned long last_check = 0;
    static bool last_state = true;
    unsigned long now = millis();

    // Only check every 5 seconds to save CPU cycles
    if (now - last_check > 5000)
    {
        last_check = now;
        bool current_state = is_connected();

        if (current_state && !last_state)
        {
            Serial.printf("MAINTAIN[WIFI] Connected! ESP's IP Address: %s\n", get_ip().c_str());
        } else if (!current_state)
        {
            if (last_state)
            {
                Serial.println("[WIFI] Network Lost. Waiting for auto-reconnect...");
            }
            WiFi.begin();
        }

        last_state = current_state;
    }
}
