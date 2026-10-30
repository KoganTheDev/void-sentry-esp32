// Fragment of the HUD IIFE (not a module). The join order is JS_FRAGMENTS in web/hud_build.py.
// ---------------- Buttons, dialogs, keyboard ----------------
$('#uncal-go').onclick = startCal;
$$('.p-mode .seg button').forEach(b => b.onclick = () => requestMode(b.dataset.mode));
$('#btn-help').onclick = () => $('#help').showModal();
const fsBtn = $('#btn-fs');
if (!document.fullscreenEnabled) fsBtn.hidden = true; // iPhone Safari has no element fullscreen
function toggleFullscreen() {
    if (!document.fullscreenEnabled) return;
    if (!document.fullscreenElement) document.documentElement.requestFullscreen().catch(() => { });
    else document.exitFullscreen();
}
fsBtn.onclick = toggleFullscreen;

const KEYS = { KeyW: 'up', ArrowUp: 'up', KeyS: 'down', ArrowDown: 'down', KeyA: 'left', ArrowLeft: 'left', KeyD: 'right', ArrowRight: 'right' };
// While a dialog is open, keys belong to it (typing a PIN must not jog or switch modes).
const dialogOpen = () => $('#help').open || $('#arm-dlg').open || $('#login-dlg').open || $('#logout-dlg').open;
window.addEventListener('keydown', (e) => {
    if (e.key === 'Shift') { setShift(true); return; }
    if (e.ctrlKey || e.metaKey || e.altKey || dialogOpen()) return;
    // e.code is layout-independent, so a layout switch mid-hold cannot strand a key.
    const dir = KEYS[e.code];
    if (dir) { e.preventDefault(); if (!e.repeat) pressDir('key:' + e.code, dir, false); return; }
    if (e.repeat) return;
    if (e.code === 'Space') { e.preventDefault(); fire(false); }
    else if (e.code === 'KeyM') toggleMode();
    else if (e.code === 'KeyC') startCal();
    else if (e.code === 'KeyH') recenter();
    else if (e.code === 'KeyP') snapshot();
    else if (e.code === 'Enter' && S.cal.active && document.activeElement.tagName !== 'BUTTON') { e.preventDefault(); record(); }
    else if (e.code === 'Escape' && logOpen) openLog(false);
    else if (e.code === 'Escape' && S.cal.active) {
        if (S.cal.confirm) { S.cal.confirm = false; renderWizard(); } else askCancel();
    }
    else if (e.code === 'KeyF') toggleFullscreen();
    else if (e.key === '?') $('#help').showModal();
});
window.addEventListener('keyup', (e) => {
    if (e.key === 'Shift') setShift(false);
    if (KEYS[e.code]) releaseDir('key:' + e.code);
});

