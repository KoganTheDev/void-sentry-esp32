#include "cal_store.h"
#include "calibration_blob.h"
#include "nvs_blob.h"
#include <Preferences.h>

namespace
{
constexpr const char* kNamespace = "aim";
constexpr const char* kKey = "map";
} // namespace

namespace cal_store
{
bool load(aiming::AimMap& out)
{
    Preferences prefs;
    if (!prefs.begin(kNamespace, true)) // read-only; fails if the namespace was never written
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
