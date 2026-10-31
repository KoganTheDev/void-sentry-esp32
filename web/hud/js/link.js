// Fragment of the HUD IIFE (not a module). The join order is JS_FRAGMENTS in web/hud_build.py.
// ---------------- /commands link ----------------
// Request/reply: the board answers every text message with exactly one JSON reply. Exactly one poll loop
// may run: the next " " poll is scheduled only when no message is awaiting its reply. Scheduling a poll
// from every reply (without that check) once doubled the loops each round trip until httpd saturated
// core 0 and the task watchdog rebooted the board (notes s8). So every send goes through sendCommand.
const App = {
    ws: null,
    inFlight: 0,        // messages sent but not yet answered
    sent: [],           // {msg, t} in send order; replies come back in the same order, so the head is answered next
    pollTimer: null,
    lastReplyMs: 0,     // for the stall watchdog (reset by it)
    lastRxMs: 0,        // last real reply, for the link state
    rtt: null,          // smoothed round trip in ms
    attempt: 0,

    sendCommand(msg) {
        const ws = this.ws;
        if (!ws || ws.readyState !== WebSocket.OPEN) return false;
        this.inFlight += 1;
        this.sent.push({ msg, t: performance.now() });
        if (this.sent.length > 64) this.sent.shift(); // a lost reply must not grow the queue forever
        ws.send(msg);
        return true;
    },

    schedulePoll() {
        if (this.inFlight === 0 && this.pollTimer === null) {
            this.pollTimer = setTimeout(() => {
                this.pollTimer = null;
                this.sendCommand(' ');
            }, 40);
        }
    },

    // True while a message starting with `prefix` is still waiting for its reply.
    awaiting(prefix) { return this.sent.some(r => r.msg.startsWith(prefix)); },

    connect() {
        const proto = location.protocol === 'https:' ? 'wss' : 'ws';
        const ws = new WebSocket(`${proto}://${location.host}/commands`);
        this.ws = ws;
        this.attempt += 1;
        $('#link-attempt').textContent = this.attempt;

        ws.onopen = () => {
            this.inFlight = 0;
            this.sent = [];
            this.lastReplyMs = performance.now();
            S.counters = null; // the board may have rebooted: take new counter baselines
            // The speed persists on the board until changed, so the HUD restores its choice on every
            // connect. This message is also the first poll: its reply starts the loop.
            this.sendCommand('speed:' + effectiveSpeed());
        };

        ws.onmessage = (ev) => {
            const now = performance.now();
            this.lastReplyMs = now;
            this.lastRxMs = now;
            this.inFlight = Math.max(0, this.inFlight - 1);
            const req = this.sent.shift();
            if (req) {
                const r = now - req.t;
                this.rtt = this.rtt === null ? r : this.rtt * 0.8 + r * 0.2;
            }
            let m = null;
            try { m = JSON.parse(ev.data); } catch (e) { m = null; }
            if (m && typeof m === 'object') {
                try { onReply(m, now); } catch (e) { console.error('reply handling failed', e); }
            }
            // Always reached, even if handling threw, so the single loop never dies.
            this.schedulePoll();
        };

        ws.onclose = () => {
            this.drop();
            setLink('lost');
            setTimeout(() => this.connect(), 1000); // the board may have rebooted
        };
    },

    // Forget the socket and everything awaiting a reply on it.
    drop() {
        clearTimeout(this.pollTimer);
        this.pollTimer = null;
        this.inFlight = 0;
        this.sent = [];
        this.ws = null;
    }
};

// No reply for 2 s on an open socket: the loop is stuck. TCP does not lose replies, so the missing one is
// only late. Resetting the counters on the same socket would let that late reply schedule a second poll
// while the restarted loop runs, and each stall would add a loop (the notes s8 storm). So the socket is
// abandoned and replaced: its late replies can never reach the new one.
setInterval(() => {
    const ws = App.ws;
    const now = performance.now();
    if (ws && ws.readyState === WebSocket.OPEN && now - App.lastReplyMs > 2000) {
        ws.onmessage = null;
        ws.onclose = null;
        ws.close();
        App.drop();
        App.connect();
    }
    // Link state: up while replies keep coming. The first 3 s after load count as "connecting".
    const open = ws && ws.readyState === WebSocket.OPEN;
    if (open && App.lastRxMs && now - App.lastRxMs < 3000) setLink('ok');
    else if (S.link === 'ok' || now - S.linkSince > 3000) setLink('lost');
}, 1000);

function setLink(state) {
    if (state === S.link) return;
    const was = S.link;
    S.link = state;
    S.linkSince = performance.now();
    if (state === 'lost') {
        App.lastRxMs = 0;
        releaseAll();
        clearGoal();
        clearPending();
        disarm('Auto-fire disarmed: link lost');
        logEvent('Turret link lost', 'err');
        announce('Turret link lost');
    } else if (state === 'ok') {
        App.attempt = 0;
        logEvent(was === 'lost' ? 'Turret link restored' : 'Turret link up', 'ok');
        announce(was === 'lost' ? 'Turret link restored' : 'Turret link up');
    }
    renderAll();
}

// ---------------- Reply handling ----------------
// Every key is optional: older firmware sends fewer keys and must not break the page.
function onReply(m, now) {
    if (S.link !== 'ok') setLink('ok');
    S.hasReply = true;
    updateTrack(m, now);
    updateAim(m, now);
    if (typeof m.lim === 'number') S.lim = m.lim;
    S.home = num(m.home);
    S.ret = num(m.ret);
    if (typeof m.vw === 'number') S.vw = m.vw;
    if (typeof m.calibrated === 'number') S.calibrated = m.calibrated === 1;
    if (typeof m.pan === 'number') S.pan = m.pan;
    if (typeof m.tilt === 'number') S.tilt = m.tilt;
    updateReach(m);
    updateCounters(m, now); // before the mode: a save and the switch to AI can arrive in the same reply
    if (typeof m.mode === 'string') updateMode(m.mode);
    updateCal(m, now);
    if (m.stick === 1 && S.held.size) toast('Joystick has control', 'warn', 'stick');
    resyncSpeed(m);
    updateGoal(now);
}

function updateTrack(m, now) {
    if (m.state === undefined && m.detected === undefined) return;
    const sx = W / (num(m.width) || W), sy = H / (num(m.height) || H);
    const prev = S.trk;
    const state = typeof m.state === 'string' ? m.state : (m.detected ? 'locked' : 'idle');
    const t = {
        state, id: num(m.id),
        x: num(m.x) * sx, y: num(m.y) * sy,
        bx: num(m.bx) * sx, by: num(m.by) * sy, bw: num(m.bw) * sx, bh: num(m.bh) * sy,
        vx: num(m.vx), vy: num(m.vy), area: num(m.pixels)
    };
    // A new target replays the bracket snap instead of showing its id (the id is never displayed).
    if (state !== 'idle' && (prev.state === 'idle' || (t.id && t.id !== prev.id))) S.acqAt = now;
    if (state === 'locked' && prev.state !== 'locked' && prev.state !== 'coasting') logEvent('Target acquired', 'ok', 'target');
    if (state === 'idle' && prev.state === 'coasting') logEvent('Target lost', 'warn', 'target');
    // Trail: the last 1.2 s of positions while the target is held.
    const ts = now / 1000;
    if (state === 'locked' || state === 'coasting') {
        const last = S.trail[S.trail.length - 1];
        if (!last || ts - last.t > 0.05) S.trail.push({ x: t.x, y: t.y, t: ts });
    } else if (state === 'idle') {
        S.trail = [];
    }
    S.trk = t;
}

function updateAim(m, now) {
    if (typeof m.ax !== 'number' || typeof m.ay !== 'number') return;
    const a = { x: m.ax, y: m.ay };
    if (S.aim && dist(a, S.aim) > 0.5) {
        // Slew speed from the observed motion of the aim pixel while it travels to a tapped point (jogs
        // run at the jog speed, so they would skew it). Used for the ghost reticle's time-left label.
        const dt = (now - S.aimSampleT) / 1000;
        if (S.uiGoal && S.held.size === 0 && dt > 0.01 && dt < 0.5) {
            S.slew = clamp(S.slew * 0.8 + (dist(a, S.aim) / dt) * 0.2, 10, 2000);
        }
        S.aimMovedAt = now;
    }
    S.aimSampleT = now;
    S.aim = a;
}

function updateReach(m) {
    const keys = ['rx0', 'ry0', 'rx1', 'ry1'];
    if (!keys.every(k => typeof m[k] === 'number')) return;
    S.reach = { x0: m.rx0, y0: m.ry0, x1: m.rx1, y1: m.ry1 };
}

// Events over a polled channel arrive as counters: a change is never missed, a one-pass flag could be.
function updateCounters(m, now) {
    const saves = typeof m.cal_saves === 'number' ? m.cal_saves : null;
    const miss = typeof m.cal_miss === 'number' ? m.cal_miss : null;
    if (S.counters === null) { S.counters = { saves, miss }; return; }
    if (saves !== null && S.counters.saves !== null && saves > S.counters.saves) calSaved(now);
    if (miss !== null && S.counters.miss !== null && miss > S.counters.miss) {
        toast('No target visible: wave at the spot', 'warn', 'cal-miss');
        logEvent('Calibration point ignored: no target visible', 'warn');
    }
    if (saves !== null) S.counters.saves = saves;
    if (miss !== null) S.counters.miss = miss;
}

function updateMode(mode) {
    const prev = S.mode;
    S.mode = mode;
    if (S.pending && (S.pending === 'cancel' ? mode !== 'calibrate' : S.pending === mode)) clearPending();
    if (mode === prev) return;
    // Held jogs and pending aims belong to the old mode.
    releaseAll();
    clearGoal();
    if (prev !== null) {
        logEvent(`Mode ${MODE_NAMES[mode] || mode}`);
        announce(`Mode ${MODE_NAMES[mode] || mode}`);
    }
    renderAll();
}

function updateCal(m, now) {
    const c = S.cal;
    const active = S.mode === 'calibrate';
    const st = typeof m.cal_state === 'string' ? m.cal_state : '';
    if (active && !c.active) {
        // Entered from the HUD or from a long press on the joystick button.
        Object.assign(c, { active: true, step: 1, p1: null, failed: '', confirm: false });
        logEvent('Calibration started', 'warn');
        announce('Calibration started, step 1 of 2');
        renderAll();
    } else if (!active && c.active) {
        c.active = false;
        c.confirm = false;
        if (now >= c.doneUntil) {
            toast(S.calibrated ? 'Calibration cancelled. Previous calibration kept.' : 'Calibration cancelled.', '', 'cal-cancel');
            logEvent('Calibration cancelled');
            announce('Calibration cancelled');
        }
        renderAll();
        return;
    }
    if (!active || now < c.doneUntil) return;
    const failed = st === 'failed' ? (typeof m.cal_error === 'string' && m.cal_error ? m.cal_error : 'Calibration failed') : '';
    const step = st === 'point2' ? 2 : 1;
    if (step === 2 && c.step !== 2) {
        // The board just recorded point 1 on the tracked target: mark it and the zone point 2 must avoid.
        // The board records once the turret has stopped, so the target may have moved since; prefer where
        // it was when Record was pressed here (a physical button press has no such sample).
        const at = c.recordAt && now - c.recordAt.t < 2000 ? c.recordAt : S.trk;
        c.p1 = { x: Math.round(at.x), y: Math.round(at.y) };
        c.recordAt = null;
        announce('Point 1 recorded. Step 2 of 2');
    }
    if (st === 'point1') c.p1 = null;
    if (failed && !c.failed) {
        logEvent('Calibration failed: ' + failed, 'err');
        announce('Calibration failed');
    }
    if (step !== c.step || failed !== c.failed) {
        c.step = step;
        c.failed = failed;
        renderWizard();
    }
}

// Success is a cal_saves increment: the firmware goes straight to AI, so cal_state never reads "done".
function calSaved(now) {
    const c = S.cal;
    c.step = 3;
    c.failed = '';
    c.confirm = false;
    c.p1 = null;
    c.doneUntil = now + 1600;
    toast('Calibration saved. AI tracking on.', 'ok', 'cal-saved');
    logEvent('Calibration saved', 'ok');
    announce('Calibration saved');
    renderWizard();
    setTimeout(renderAll, 1650);
}

// Another HUD may change the jog speed: follow the board's `spd`, except while our own change (or a held
// Shift) is in flight, whose reply is the authoritative one.
function resyncSpeed(m) {
    if (typeof m.spd !== 'string' || !SPEEDS.includes(m.spd)) return;
    if (S.shift || App.awaiting('speed:')) return;
    if (m.spd !== S.speed) { S.speed = m.spd; renderSpeed(); }
}

