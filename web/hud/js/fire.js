// Fragment of the HUD IIFE (not a module). The join order is JS_FRAGMENTS in web/hud_build.py.
// ---------------- Fire (HUD effect only: no actuator, the HUD never sends a fire command) ----------------
function trackBox(t) {
    if (t.bw > 0 && t.bh > 0) return { x: t.bx, y: t.by, w: t.bw, h: t.bh };
    return { x: t.x - 22, y: t.y - 37, w: 44, h: 74 };
}
// On target: the real reticle is inside the box (+6 px) of a locked or coasting target.
function onTarget() {
    if (!S.aim || !trackUsable() || S.link !== 'ok') return false;
    const b = trackBox(S.trk), a = S.aim;
    return a.x >= b.x - 6 && a.x <= b.x + b.w + 6 && a.y >= b.y - 6 && a.y <= b.y + b.h + 6;
}
function fire(fromTap) {
    const now = performance.now();
    if (S.link !== 'ok') return toast('No turret link', 'warn', 'link');
    if (needControl()) return;
    if (now < S.cooldownUntil) return;
    S.cooldownUntil = now + 700;
    S.charge = 0;
    const hit = onTarget();
    const at = S.aim ? { x: clamp(S.aim.x, 0, W), y: clamp(S.aim.y, 0, H) } : { x: HOME.x, y: HOME.y };
    const t1 = S.reduced ? now : now + 120;
    if (!S.reduced) {
        frameEl.classList.add('flash');
        setTimeout(() => frameEl.classList.remove('flash'), 70);
        S.fx.push({ type: 'tracer', t0: now, x0: 160, y0: 244, x1: at.x, y1: at.y });
    }
    if (hit) {
        if (fromTap) haptic([18, 40, 18]);
        if (!S.reduced) {
            S.fx.push({ type: 'ring', t0: t1, x: at.x, y: at.y });
            for (let i = 0; i < 9; i++) {
                const a = Math.random() * Math.PI * 2, sp = 40 + Math.random() * 70;
                S.fx.push({ type: 'spark', t0: t1, x: at.x, y: at.y, vx: Math.cos(a) * sp, vy: Math.sin(a) * sp });
            }
            S.boxFlashUntil = t1 + 160;
        }
        S.fx.push({ type: 'hit', t0: t1 });
        logEvent('Target hit', 'hit', 'target');
    }
}
const fireBtn = $('#fire');
fireBtn.addEventListener('pointerdown', (e) => { e.preventDefault(); fire(true); });
fireBtn.addEventListener('click', (e) => { if (e.detail === 0) fire(false); }); // keyboard activation
fireBtn.addEventListener('contextmenu', (e) => e.preventDefault());

// Arming auto-fire always asks first.
const autoBox = $('#auto-fire');
autoBox.addEventListener('click', (e) => {
    if (autoBox.checked && needControl()) { e.preventDefault(); return; }
    if (autoBox.checked) { e.preventDefault(); $('#arm-dlg').showModal(); }
    else disarm('Auto-fire off');
});
$('#arm-dlg').addEventListener('close', () => {
    if ($('#arm-dlg').returnValue === 'arm') {
        S.armed = true;
        autoBox.checked = true;
        $('#chip-armed').hidden = false;
        toast('Auto-fire armed', 'warn');
        logEvent('Auto-fire armed', 'warn');
    }
    $('#arm-dlg').returnValue = '';
});
function disarm(msg) {
    if (!S.armed) return;
    S.armed = false;
    S.charge = 0;
    autoBox.checked = false;
    $('#chip-armed').hidden = true;
    if (msg) { toast(msg, ''); logEvent(msg); }
}

