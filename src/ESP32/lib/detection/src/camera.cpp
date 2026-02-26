#include "camera.h"

#include <Arduino.h>
#include <esp_log.h>

static const char* TAG = "CAMERA";

bool Camera::begin()
{
    ESP_LOGI(TAG, "Starting camera initialization...");

    if (!psramFound())
    {
        ESP_LOGE(TAG, "CRITICAL ERROR: PSRAM not found! Camera requires PSRAM.");
        return false;
    }

    uint32_t psram_free = ESP.getFreePsram();
    uint32_t psram_total = ESP.getPsramSize();
    ESP_LOGI(TAG, "PSRAM Status - Free: %u bytes, Total: %u bytes (%.1f%% used)", psram_free, psram_total,
             (100.0f * (psram_total - psram_free) / psram_total));

    if (psram_free < 2000000) // Warn if less than 2MB free
    {
        ESP_LOGW(TAG, "WARNING: Low PSRAM available! May cause frame buffer issues.");
    }

    esp_err_t err = ESP_FAIL;
    ESP_LOGD(TAG, "Calling esp_camera_init()...");
    err = esp_camera_init(&_config);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_camera_init() failed with error code: 0x%X (%s)", err, esp_err_to_name(err));
        return false;
    }

    // Get sensor and configure for OV3660
    sensor_t* s = esp_camera_sensor_get();
    if (s == NULL)
    {
        Serial.println("ERROR: Failed to get sensor. Camera module not detected.");
        return false;
    }

    // Configure OV3660 specific settings for image clarity
    s->set_vflip(s, 1);      // Flip image upside-down
    s->set_brightness(s, 1); // Slight brightness boost reduces noise in low light
    s->set_contrast(s, 2);   // Boost contrast for clearer edges (better for motion detection)
    s->set_saturation(s, 1); // Slight saturation for color clarity

    return true;
}

camera_fb_t* Camera::capture() { return esp_camera_fb_get(); }

void Camera::release(camera_fb_t* fb)
{
    if (fb == NULL)
    {
        ESP_LOGW(TAG, "Attempted to release NULL frame buffer");
        return;
    }
    esp_camera_fb_return(fb);
    ESP_LOGV(TAG, "Frame buffer released");
}
