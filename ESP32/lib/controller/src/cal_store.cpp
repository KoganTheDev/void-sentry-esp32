#include "cal_store.h"
#include "calibration_blob.h"
#include "nvs_blob.h"
#include <Preferences.h>
#include <nvs.h>

namespace
{
constexpr const char* kNamespace = "aim";
constexpr const char* kKey = "map";
} // namespace

namespace cal_store
{
bool load(aiming::AimMap& out)
{
    // A namespace that was never written is the normal first boot. Preferences logs an [E] for it, so probe with the
    // IDF call first, which reports ESP_ERR_NVS_NOT_FOUND silently.
    nvs_handle_t probe = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &probe) != ESP_OK)
    {
        return false;
    }
    nvs_close(probe);

    Preferences prefs;
    if (!prefs.begin(kNamespace, true))
    {
        return false;
    }
    aiming::Blob buf{};
    bool ok = false;
    if (nvs_blob::read(prefs, kKey, buf) == nvs_blob::ReadStatus::Ok)
    {
        const std::optional<aiming::AimMap> map = aiming::deserialize(buf);
        if (map)
        {
            out = *map;
            ok = true;
        }
    }
    prefs.end();
    return ok;
}

bool save(const aiming::AimMap& map)
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, false))
    {
        return false;
    }
    const aiming::Blob blob = aiming::serialize(map);
    const size_t written = prefs.putBytes(kKey, blob.data(), blob.size());
    prefs.end();
    return written == blob.size();
}
} // namespace cal_store
