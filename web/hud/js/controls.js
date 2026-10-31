// Fragment of the HUD IIFE (not a module). The join order is JS_FRAGMENTS in web/hud_build.py.
// ---------------- Mode and calibration requests ----------------
function setPending(target) {
    S.pending = target;
    clearTimeout(S.pendingTimer);
    S.pendingTimer = setTimeout(() => {
        if (S.pending !== target) return;
        S.pending = null;
        toast('Turret did not switch', 'warn', 'switch');
        logEvent('Turret did not switch', 'warn');
        renderMode();
    }, 1500);
    renderMode();
}
function clearPending() {
    clearTimeout(S.pendingTimer);
    S.pendingTimer = null;
    if (S.pending) { S.pending = null; renderMode(); }
}

function requestMode(m) {
    if (S.link !== 'ok') return toast('No turret link', 'warn', 'link');
    if (needControl()) return;
    if (m === 'calibrate') return startCal();
    if (S.cal.active) return toast('Finish or cancel calibration first', 'warn', 'cal-busy');
    if (m === S.mode || S.pending) return;
    // The board owns the mode: show "switching" until a reply confirms it.
    if (App.sendCommand('mode:' + m)) setPending(m);
}
function toggleMode() {
    if (needControl()) return;
    if (S.cal.active) return toast('Finish or cancel calibration first', 'warn', 'cal-busy');
    if (S.mode) requestMode(S.mode === 'ai' ? 'user' : 'ai');
}
function startCal() {
    if (S.link !== 'ok') return toast('No turret link', 'warn', 'link');
    if (needControl()) return;
    if (S.cal.active || S.pending) return;
    if (App.sendCommand('cal:start')) setPending('calibrate');
}

function trackUsable() { return S.trk.state === 'locked' || S.trk.state === 'coasting'; }
function turretStill() {
    return S.held.size === 0 && !S.uiGoal && performance.now() - S.aimMovedAt > 300;
}
function recordBlock() {
    if (S.link !== 'ok') return 'No turret link';
    if (!canControl()) return 'View only: log in to record';
    if (!trackUsable()) return 'No target visible: wave at the spot';
    if (!turretStill()) return 'Wait for the turret to stop';
    return '';
}
function record() {
    const c = S.cal;
    if (!c.active || c.confirm || c.step === 3) return;
    if (needControl()) return;
    if (c.failed) { App.sendCommand('cal:point'); return; } // restarts at point 1
    const why = recordBlock();
    if (why) return toast(why, 'warn', 'rec');
    c.recordAt = { x: S.trk.x, y: S.trk.y, t: performance.now() };
    App.sendCommand('cal:point');
}
function askCancel() {
    if (needControl()) return;
    if (S.cal.active && S.cal.step !== 3) { S.cal.confirm = true; renderWizard(); }
}
function cancelCal() {
    if (needControl()) return;
    S.cal.confirm = false;
    if (App.sendCommand('cal:cancel')) setPending('cancel');
    renderWizard();
}

// ---------------- Jog ----------------
function canJog() { return S.link === 'ok' && canControl() && (S.mode === 'user' || S.mode === 'calibrate'); }
function atLimit(dir) { return (S.lim & (LIM_BITS[dir] || 0)) !== 0; }
function effectiveSpeed() { return S.shift ? 'slow' : S.speed; }
function sendSpeed() { App.sendCommand('speed:' + effectiveSpeed()); }

function pressDir(token, dir, fromTap) {
    if (!canJog()) {
        if (S.link === 'ok' && needControl()) return;
        if (S.link === 'ok') {
            const d = $('#dpad');
            d.classList.remove('shake');
            void d.offsetWidth; // restart the animation
            d.classList.add('shake');
            toast('Switch to Manual (M) to jog', 'warn', 'jog-ai');
        } else {
            toast('No turret link', 'warn', 'link');
        }
        return;
    }
    if (atLimit(dir)) {
        if (fromTap) haptic([30]);
        toast(`${dir === 'left' || dir === 'right' ? 'Pan' : 'Tilt'} limit reached`, 'warn', 'limit');
    }
    clearGoal();
    S.held.set(token, dir);
    refreshMove();
}
function releaseDir(token) { S.held.delete(token); refreshMove(); }
function releaseAll() { S.held.clear(); refreshMove(); }

// Held directions from keys and on-screen buttons share one map. The combined direction is sent on
// change and resent every 100 ms (the firmware stops jogging 300 ms after the last move message).
function combinedDir() {
    const dirs = new Set(S.held.values());
    const v = (dirs.has('up') ? 1 : 0) - (dirs.has('down') ? 1 : 0);
    const h = (dirs.has('right') ? 1 : 0) - (dirs.has('left') ? 1 : 0);
    const name = (v > 0 ? 'up' : v < 0 ? 'down' : '') + (v && h ? '-' : '') + (h > 0 ? 'right' : h < 0 ? 'left' : '');
    return name || null;
}
function refreshMove() {
    const dirs = new Set(S.held.values());
    $$('.dpad button[data-dir]').forEach(b => b.classList.toggle('held', dirs.has(b.dataset.dir)));
    const dir = combinedDir();
    if (dir === S.moveDir) return;
    S.moveDir = dir;
    if (dir === null) {
        clearInterval(S.moveTimer);
        S.moveTimer = null;
        App.sendCommand('move:stop');
        return;
    }
    App.sendCommand('move:' + dir);
    if (S.moveTimer === null) {
        S.moveTimer = setInterval(() => {
            if (S.moveDir !== null) App.sendCommand('move:' + S.moveDir);
        }, 100);
    }
}

$$('.dpad button[data-dir]').forEach(b => {
    const d = b.dataset.dir, token = 'btn:' + d;
    b.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        b.setPointerCapture(e.pointerId);
        pressDir(token, d, true);
    });
    ['pointerup', 'pointercancel', 'lostpointercapture'].forEach(ev => b.addEventListener(ev, () => {
        if (S.held.has(token)) releaseDir(token);
    }));
    b.addEventListener('contextmenu', (e) => e.preventDefault());
});
// Never leave the turret jogging when the page loses focus.
window.addEventListener('blur', () => { releaseAll(); setShift(false); });
document.addEventListener('visibilitychange', () => {
    if (document.hidden) { releaseAll(); S.dragId = null; }
});

function renderSpeed() {
    $$('[data-speed]').forEach(x => x.setAttribute('aria-pressed', String(x.dataset.speed === S.speed)));
}
$$('[data-speed]').forEach(b => b.onclick = () => {
    if (needControl() || S.speed === b.dataset.speed) return;
    S.speed = b.dataset.speed;
    renderSpeed();
    sendSpeed();
});
function setShift(on) {
    if (S.shift === on) return;
    S.shift = on;
    if (canControl()) sendSpeed(); // a viewer's speed change would only be dropped by the board
}

// ---------------- Tap or drag to aim ----------------
// The dashed ghost (uiGoal) follows the finger at once; the solid reticle only moves with the board's
// ax/ay. The board gets the newest point at most every 100 ms (latest wins, nothing queued, so the turret
// never replays stale points) and the release point is always sent.
function clearGoal() {
    S.uiGoal = null;
    S.dragId = null;
    clearTimeout(S.aimTimer);
    S.aimTimer = null;
}
function reachBox() { return S.reach || { x0: 0, y0: 0, x1: W - 1, y1: H - 1 }; }
function clampReach(p) {
    const r = reachBox();
    return { x: clamp(p.x, r.x0, r.x1), y: clamp(p.y, r.y0, r.y1) };
}
function outOfReach(p) { return dist(clampReach(p), p) > 0.5; }
function framePoint(e) {
    const r = $('#frame').getBoundingClientRect();
    return {
        x: clamp((e.clientX - r.left) / r.width * W, 0, W - 1),
        y: clamp((e.clientY - r.top) / r.height * H, 0, H - 1)
    };
}
function sendAim(force) {
    if (!S.uiGoal) return;
    const now = performance.now();
    const wait = 100 - (now - S.lastAimSend);
    if (!force && wait > 0) {
        // Throttled: send whatever the newest point is when the window ends.
        if (S.aimTimer === null) S.aimTimer = setTimeout(() => { S.aimTimer = null; sendAim(false); }, wait);
        return;
    }
    clearTimeout(S.aimTimer);
    S.aimTimer = null;
    const x = clamp(Math.round(S.uiGoal.x), 0, 319), y = clamp(Math.round(S.uiGoal.y), 0, 239);
    const msg = `aim:${x},${y}`;
    if (!force && msg === S.lastAimMsg) return;
    if (App.sendCommand(msg)) { S.lastAimSend = now; S.lastAimMsg = msg; }
}
function aimAt(p) {
    if (needControl()) return;
    if (!canJog()) return toast('Switch to Manual (M) to aim', 'warn', 'tap-ai');
    releaseAll();
    S.uiGoal = { x: p.x, y: p.y };
    S.fx.push({ type: 'ping', t0: performance.now(), x: p.x, y: p.y });
    sendAim(true);
}
// The ghost goes away when the real reticle arrives, or when the turret has stopped short of it.
function updateGoal(now) {
    if (!S.uiGoal || S.dragId !== null || !S.aim) return;
    const stop = clampReach(S.uiGoal);
    const out = outOfReach(S.uiGoal);
    const arrived = dist(S.aim, stop) <= 3;
    const stalled = now - S.lastAimSend > 1500 && now - S.aimMovedAt > 1200;
    if (arrived || stalled) {
        if (out) toast('Out of reach: stopped at the limit', 'warn', 'reach');
        clearGoal();
    }
}

const frameEl = $('#frame');
frameEl.addEventListener('pointerdown', (e) => {
    if (S.link !== 'ok') return toast('No turret link', 'warn', 'link');
    if (needControl()) return;
    if (S.mode === 'ai') return toast('Switch to Manual (M) to aim by tapping', 'warn', 'tap-ai');
    if (!canJog()) return;
    e.preventDefault();
    frameEl.setPointerCapture(e.pointerId);
    aimAt(framePoint(e));
    S.dragId = e.pointerId;
});
frameEl.addEventListener('pointermove', (e) => {
    if (S.dragId !== e.pointerId || !S.uiGoal) return;
    S.uiGoal = framePoint(e);
    sendAim(false);
});
['pointerup', 'pointercancel', 'lostpointercapture'].forEach(ev => frameEl.addEventListener(ev, (e) => {
    if (S.dragId !== e.pointerId) return;
    S.dragId = null;
    sendAim(true); // the release point always reaches the board
}));
function recenter() {
    if (S.link !== 'ok') return toast('No turret link', 'warn', 'link');
    if (needControl()) return;
    aimAt(HOME);
}
$('#recenter').addEventListener('click', recenter);

