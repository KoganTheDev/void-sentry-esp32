#include "wifi_store.h"

#include "nvs_blob.h"

#include <Preferences.h>
#include <esp_system.h>

static const char* TAG_STORE = "WIFI_STORE";
static const char* kNamespace = "wifi";
static const char* kKeyNets = "nets";
static const char* kKeyPin = "pin";
static const char* kKeyAppw = "appw";
static constexpr size_t kHotspotPassLen = 10;

bool WifiStore::load(prov::NetworkList& list, prov::PinRecord& pin)
{
    list.clear();
    pin = prov::PinRecord{};

    Preferences prefs;
    if (!prefs.begin(kNamespace, false))
    {
        ESP_LOGE(TAG_STORE, "NVS namespace open failed");
        return false;
    }

    bool nets_ok = false;
    std::array<uint8_t, prov::kNetsBlobSize> nets_buf{};
    const auto nets_status = nvs_blob::read(prefs, kKeyNets, nets_buf);
    if (nets_status != nvs_blob::ReadStatus::Absent)
    {
        if (nets_status == nvs_blob::ReadStatus::Ok)
        {
            const auto parsed = prov::deserialize_nets(nets_buf);
            if (parsed)
            {
                list = *parsed;
                nets_ok = true;
            }
        }
        if (!nets_ok)
        {
            ESP_LOGW(TAG_STORE, "Saved network list is corrupt, treating as empty");
        }
    }

    std::array<uint8_t, prov::kPinBlobSize> pin_buf{};
    const auto pin_status = nvs_blob::read(prefs, kKeyPin, pin_buf);
    if (pin_status != nvs_blob::ReadStatus::Absent)
    {
        bool pin_ok = false;
        if (pin_status == nvs_blob::ReadStatus::Ok)
        {
            const auto parsed = prov::deserialize_pin(pin_buf);
            if (parsed)
            {
                pin = *parsed;
                pin_ok = true;
            }
        }
        if (!pin_ok)
        {
            ESP_LOGW(TAG_STORE, "Saved PIN record is corrupt, treating as not set");
        }
    }
    prefs.end();
    return nets_ok;
}

bool WifiStore::save_nets(const prov::NetworkList& list)
{
    const auto blob = prov::serialize_nets(list);
    Preferences prefs;
    if (!prefs.begin(kNamespace, false))
    {
        return false;
    }
    const bool ok = prefs.putBytes(kKeyNets, blob.data(), blob.size()) == blob.size();
    prefs.end();
    if (!ok)
    {
        ESP_LOGE(TAG_STORE, "Saving the network list failed");
    }
    return ok;
}

bool WifiStore::save_pin(const prov::PinRecord& rec)
{
    const auto blob = prov::serialize_pin(rec);
    Preferences prefs;
    if (!prefs.begin(kNamespace, false))
    {
        return false;
    }
    const bool ok = prefs.putBytes(kKeyPin, blob.data(), blob.size()) == blob.size();
    prefs.end();
    if (!ok)
    {
        ESP_LOGE(TAG_STORE, "Saving the PIN record failed");
    }
    return ok;
}

String WifiStore::hotspot_password()
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, false))
    {
        return String();
    }
    String pw = prefs.getString(kKeyAppw, "");
    if (pw.length() != kHotspotPassLen)
    {
        // 57 symbols: no 0, O, 1, l, I (easy to misread when typed from a serial log into a phone).
        static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
        constexpr uint32_t n = sizeof(alphabet) - 1;
        pw = "";
        while (pw.length() < kHotspotPassLen)
        {
            const uint32_t r = esp_random() & 0x3F; // rejection sampling keeps the draw unbiased
            if (r < n)
            {
                pw += alphabet[r];
            }
        }
        prefs.putString(kKeyAppw, pw);
    }
    prefs.end();
    return pw;
}

void WifiStore::erase_all()
{
    Preferences prefs;
    if (prefs.begin(kNamespace, false))
    {
        prefs.remove(kKeyNets);
        prefs.remove(kKeyPin);
        // Rotate the hotspot password too: a missing key makes hotspot_password() generate a fresh one, so a password
        // that leaked from an old serial log stops working after a reset.
        prefs.remove(kKeyAppw);
        prefs.end();
    }
}
