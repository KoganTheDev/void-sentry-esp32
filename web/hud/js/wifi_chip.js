// Fragment of the HUD IIFE (not a module). The join order is JS_FRAGMENTS in web/hud_build.py.
// ---------------- WiFi chip ----------------
// Shows the connected SSID (or the hotspot mode); read-only, independent of /commands.
function refreshWifi() {
    fetch('/api/wifi/status')
        .then((r) => r.json())
        .then((s) => {
            const text = s && s.ssid ? s.ssid : (s && s.mode) || '-';
            setChip('#chip-wifi', s && s.ssid ? 'ok' : 'warn', text);
            $('#chip-wifi').title = `WiFi setup (${text})`; // the chip may cut a long SSID
        })
        .catch(() => setChip('#chip-wifi', '', '-'));
}

