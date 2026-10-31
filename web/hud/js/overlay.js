// Fragment of the HUD IIFE (not a module). The join order is JS_FRAGMENTS in web/hud_build.py.
// ---------------- Overlay drawing ----------------
const ovCanvas = $('#overlay');
const ov = ovCanvas.getContext('2d');
// Back the overlay with device pixels so lines stay sharp on large and high-DPI screens. Drawing is in
// camera pixels; the transform scales it.
function sizeOverlay() {
    const r = frameEl.getBoundingClientRect();
    const w = Math.max(320, Math.round(r.width * (window.devicePixelRatio || 1)));
    if (ovCanvas.width !== w) {
        ovCanvas.width = w;
        ovCanvas.height = Math.round(w * 3 / 4);
    }
}
if (window.ResizeObserver) new ResizeObserver(sizeOverlay).observe(frameEl);
else window.addEventListener('resize', sizeOverlay);

// Classic reticle: ring, ticks and a centre dot, red over a black under-stroke so it reads on any scene.
function drawReticle(g, x, y, dashed) {
    const pass = (under) => {
        g.strokeStyle = g.fillStyle = under ? RET_BLACK : RET_RED;
        g.lineWidth = under ? 3.6 : 1.5;
        g.setLineDash(dashed && !under ? [3, 3] : []);
        g.beginPath(); g.arc(x, y, 9, 0, Math.PI * 2); g.stroke();
        g.beginPath();
        g.moveTo(x - 15, y); g.lineTo(x - 5, y); g.moveTo(x + 5, y); g.lineTo(x + 15, y);
        g.moveTo(x, y - 15); g.lineTo(x, y - 5); g.moveTo(x, y + 5); g.lineTo(x, y + 15);
        g.stroke();
        g.beginPath(); g.arc(x, y, under ? 2.4 : 1.3, 0, Math.PI * 2); g.fill();
    };
    g.save();
    g.lineCap = 'round';
    pass(true);
    pass(false);
    g.restore();
}

function bracket(g, x, y, w, h, len) {
    g.beginPath();
    g.moveTo(x, y + len); g.lineTo(x, y); g.lineTo(x + len, y);
    g.moveTo(x + w - len, y); g.lineTo(x + w, y); g.lineTo(x + w, y + len);
    g.moveTo(x + w, y + h - len); g.lineTo(x + w, y + h); g.lineTo(x + w - len, y + h);
    g.moveTo(x + len, y + h); g.lineTo(x, y + h); g.lineTo(x, y + h - len);
    g.stroke();
}

function drawOverlay(now) {
    const g = ov, k = ovCanvas.width / W;
    g.setTransform(k, 0, 0, k, 0, 0);
    g.clearRect(0, 0, W, H);
    if (!S.hasReply) return;
    g.lineCap = 'round';
    g.lineJoin = 'round';
    const t = S.trk, c = S.cal;

    // Calibration aids: point 1 and the zone point 2 must stay out of (80 x 60 px each side).
    if (c.active && c.p1 && now >= c.doneUntil) {
        const p = c.p1;
        if (c.step === 2 && !c.failed) {
            g.fillStyle = 'rgba(255,77,77,0.12)';
            g.fillRect(p.x - 80, p.y - 60, 160, 120);
            g.setLineDash([4, 3]); g.strokeStyle = 'rgba(255,77,77,0.6)'; g.lineWidth = 1;
            g.strokeRect(p.x - 80, p.y - 60, 160, 120); g.setLineDash([]);
            g.fillStyle = 'rgba(255,120,120,0.9)'; g.font = '600 7px ui-monospace, monospace';
            // Label inside the zone, at its bottom edge when the top is off-frame or under the state badge.
            const zoneTop = p.y - 60, labelY = zoneTop < 24 ? Math.min(H - 4, p.y + 56) : zoneTop + 8;
            g.fillText('TOO CLOSE FOR POINT 2', Math.max(2, p.x - 78), labelY);
        }
        g.strokeStyle = '#ffb020'; g.lineWidth = 2;
        g.beginPath(); g.moveTo(p.x - 5, p.y - 5); g.lineTo(p.x + 5, p.y + 5); g.moveTo(p.x + 5, p.y - 5); g.lineTo(p.x - 5, p.y + 5); g.stroke();
        g.fillStyle = '#ffb020'; g.font = '700 8px ui-monospace, monospace'; g.fillText('P1', p.x + 7, p.y - 5);
    }

    // A thin, soft trail of the last 1.2 s; low contrast so it is easy to watch for long.
    const ts = now / 1000;
    while (S.trail.length && ts - S.trail[0].t > 1.2) S.trail.shift();
    for (let i = 1; i < S.trail.length; i++) {
        const a = S.trail[i - 1], b = S.trail[i];
        const age = (ts - b.t) / 1.2;
        g.strokeStyle = `rgba(214,196,178,${0.3 * (1 - age)})`;
        g.lineWidth = 1.2;
        g.beginPath(); g.moveTo(a.x, a.y); g.lineTo(b.x, b.y); g.stroke();
    }

    // Target brackets: white with a dark halo; amber dashed while coasting, light dotted while acquiring.
    // A new target replays a short snap (from 1.4x size, ease-out with overshoot).
    const on = onTarget();
    const box = trackBox(t);
    if (t.state !== 'idle') {
        const ae = now - S.acqAt;
        const grow = S.reduced || ae > 280 ? 1 : 1.4 - 0.4 * easeOutBack(ae / 280);
        const bw = box.w * grow, bh = box.h * grow;
        const bx = box.x + box.w / 2 - bw / 2, by = box.y + box.h / 2 - bh / 2;
        const len = Math.max(3, Math.min(10, bw / 3, bh / 3));
        const flash = now < S.boxFlashUntil;
        const col = flash ? '#ff4d4d' : t.state === 'locked' ? '#f2f2f2' : t.state === 'coasting' ? '#ffb020' : '#d0d0d0';
        g.setLineDash(t.state === 'coasting' ? [5, 4] : t.state === 'tentative' ? [2, 3] : []);
        g.strokeStyle = 'rgba(0,0,0,0.65)'; g.lineWidth = 4; bracket(g, bx, by, bw, bh, len);
        g.strokeStyle = col; g.lineWidth = 2; bracket(g, bx, by, bw, bh, len);
        g.setLineDash([]);
        // On-target cue: inner brackets snap in once (300 ms ease-out with a small overshoot), then
        // breathe (symmetric sine ease-in-out, 1.6 s per cycle, 3 px of travel).
        if (on) {
            if (!S.lockSince) S.lockSince = now;
            const el = now - S.lockSince;
            let ins;
            if (S.reduced) ins = 6;
            else if (el < 300) ins = 6 * easeOutBack(el / 300);
            else ins = 6 + 1.5 * (1 - Math.cos(((el - 300) / 1600) * 2 * Math.PI));
            ins = Math.min(ins, bw / 2 - 2, bh / 2 - 2);
            g.globalAlpha = S.reduced ? 1 : Math.min(1, el / 200);
            g.strokeStyle = RET_RED; g.lineWidth = 1.5;
            if (ins > 0) bracket(g, bx + ins, by + ins, bw - 2 * ins, bh - 2 * ins, Math.max(2, len - 4));
            g.font = '800 8px system-ui, sans-serif';
            const txt = 'ON TARGET', w2 = g.measureText(txt).width + 8;
            const cx = box.x + box.w / 2, ly = Math.min(H - 12, by + bh + 4);
            g.fillStyle = '#d61f1f'; g.fillRect(cx - w2 / 2, ly, w2, 11);
            g.fillStyle = '#fff'; g.fillText(txt, cx - w2 / 2 + 4, ly + 8.5);
            g.globalAlpha = 1;
        }
    }
    if (!on) S.lockSince = 0;
    fireBtn.classList.toggle('ready', on);

    // While dragging: the faint reach box (what the turret can physically point at).
    if (S.dragId !== null && S.reach) {
        const r = S.reach;
        g.strokeStyle = 'rgba(255,255,255,0.12)'; g.lineWidth = 1; g.setLineDash([4, 4]);
        g.strokeRect(r.x0, r.y0, r.x1 - r.x0, r.y1 - r.y0);
        g.setLineDash([]);
    }
    // Ghost reticle where you asked, a dotted line to the real reticle, and the time left. Out of reach:
    // amber, with an x where the turret will stop.
    if (S.uiGoal) {
        const q = S.uiGoal;
        const stop = clampReach(q);
        const out = outOfReach(q);
        const col = out ? '255,176,32' : RET_RGB;
        g.strokeStyle = `rgba(${col},0.6)`; g.lineWidth = 1; g.setLineDash([2, 3]);
        if (S.aim) { g.beginPath(); g.moveTo(clamp(S.aim.x, 0, W), clamp(S.aim.y, 0, H)); g.lineTo(q.x, q.y); g.stroke(); }
        g.beginPath(); g.arc(q.x, q.y, 9, 0, Math.PI * 2); g.stroke();
        g.setLineDash([]);
        g.beginPath(); g.moveTo(q.x - 3, q.y); g.lineTo(q.x + 3, q.y); g.moveTo(q.x, q.y - 3); g.lineTo(q.x, q.y + 3); g.stroke();
        if (out) {
            g.strokeStyle = 'rgba(255,176,32,0.95)'; g.lineWidth = 1.5;
            g.beginPath(); g.moveTo(stop.x - 4, stop.y - 4); g.lineTo(stop.x + 4, stop.y + 4); g.moveTo(stop.x + 4, stop.y - 4); g.lineTo(stop.x - 4, stop.y + 4); g.stroke();
        }
        const left = S.aim ? dist(stop, S.aim) : 0;
        if (S.aim && (left > 3 || out)) {
            // Distance over the observed slew speed, plus half the round trip: the reply we draw from
            // is that old, and the command takes that long to arrive.
            const eta = left / S.slew + (App.rtt || 0) / 2000;
            const label = out ? `OUT OF REACH · ${eta.toFixed(1)} s` : `${eta.toFixed(1)} s`;
            g.font = '700 7.5px ui-monospace, monospace';
            const lw = g.measureText(label).width + 6;
            const lx = Math.min(W - lw - 2, q.x + 12), ly = Math.max(10, q.y - 12);
            g.fillStyle = 'rgba(0,0,0,0.72)'; g.fillRect(lx, ly - 8, lw, 10);
            g.fillStyle = `rgb(${col})`; g.fillText(label, lx + 3, ly - 0.5);
        }
    }

    // Aim reticle: where the turret points (the board's inverse calibration map). The board does not
    // clamp ax/ay, so an aim outside the frame is drawn at the edge.
    if (S.aim) {
        const ax = clamp(S.aim.x, 0, W), ay = clamp(S.aim.y, 0, H);
        const uncal = S.calibrated === false;
        g.globalAlpha = uncal ? 0.6 : 1;
        drawReticle(g, ax, ay, uncal);
        if (uncal) {
            // Approximate aim without a calibration: dashed reticle plus a "~".
            g.font = '800 9px ui-monospace, monospace';
            g.lineWidth = 2.5; g.strokeStyle = RET_BLACK; g.strokeText('~', ax + 14, ay + 16);
            g.fillStyle = RET_RED; g.fillText('~', ax + 14, ay + 16);
        }
        g.globalAlpha = 1;
        // Charge ring before an automatic shot.
        if (S.charge > 0) {
            g.strokeStyle = '#ff6b00'; g.lineWidth = 2.5;
            g.beginPath(); g.arc(ax, ay, 16, -Math.PI / 2, -Math.PI / 2 + S.charge * Math.PI * 2); g.stroke();
        }
    }

    // Shot and tap effects.
    const hitY = (t.state !== 'idle' ? box.y : HOME.y) - 16;
    const hitX = t.state !== 'idle' ? box.x + box.w / 2 : HOME.x;
    S.fx = S.fx.filter(f => {
        const e = now - f.t0;
        if (e < 0) return true;
        if (f.type === 'tracer') {
            if (e > 160) return false;
            const p = Math.min(1, e / 120), q = Math.max(0, p - 0.45);
            const hx = f.x0 + (f.x1 - f.x0) * p, hy = f.y0 + (f.y1 - f.y0) * p;
            const tx = f.x0 + (f.x1 - f.x0) * q, ty = f.y0 + (f.y1 - f.y0) * q;
            g.save(); g.shadowColor = '#ffd27a'; g.shadowBlur = 8;
            g.strokeStyle = `rgba(255,240,190,${1 - Math.max(0, e - 120) / 40})`; g.lineWidth = 2;
            g.beginPath(); g.moveTo(tx, ty); g.lineTo(hx, hy); g.stroke(); g.restore();
            return true;
        }
        if (f.type === 'ring') {
            if (e > 380) return false;
            const p = e / 380;
            g.strokeStyle = `rgba(255,120,60,${1 - p})`;
            g.lineWidth = 2.5 * (1 - p) + 0.5;
            g.beginPath(); g.arc(f.x, f.y, 3 + p * 26, 0, Math.PI * 2); g.stroke();
            return true;
        }
        if (f.type === 'spark') {
            if (e > 420) return false;
            const s = e / 1000, p = e / 420;
            const x = f.x + f.vx * s, y = f.y + f.vy * s + 60 * s * s;
            g.fillStyle = `rgba(255,${Math.round(180 - 100 * p)},60,${1 - p})`;
            g.beginPath(); g.arc(x, y, 1.6, 0, Math.PI * 2); g.fill();
            return true;
        }
        if (f.type === 'hit') {
            // "HIT!" rises from just above the target and fades out as it goes.
            const dur = 1000;
            if (e > dur) return false;
            const p = e / dur;
            const rise = S.reduced ? 0 : p * 26;
            const pop = S.reduced ? 1 : 1 + 0.35 * Math.max(0, 1 - e / 120);
            g.save();
            g.globalAlpha = 1 - p * p;
            g.translate(hitX, Math.max(16, hitY - rise)); g.scale(pop, pop);
            g.font = '900 17px system-ui, sans-serif';
            g.textAlign = 'center';
            g.lineWidth = 3.5; g.strokeStyle = 'rgba(0,0,0,0.85)';
            g.strokeText('HIT!', 0, 0);
            const grad = g.createLinearGradient(0, -14, 0, 2);
            grad.addColorStop(0, '#fff3c4'); grad.addColorStop(1, '#ff5a1f');
            g.fillStyle = grad; g.fillText('HIT!', 0, 0);
            g.restore();
            return true;
        }
        if (f.type === 'ping') {
            if (e > 500) return false;
            const p = e / 500;
            g.strokeStyle = `rgba(${RET_RGB},${1 - p})`; g.lineWidth = 1.5;
            g.beginPath(); g.arc(f.x, f.y, 3 + p * 14, 0, Math.PI * 2); g.stroke();
            return true;
        }
        return false;
    });
}

