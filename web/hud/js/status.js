// Fragment of the HUD IIFE (not a module). The join order is JS_FRAGMENTS in web/hud_build.py.
// ---------------- Status rendering ----------------
function setChip(id, cls, text) {
    const c = $(id);
    const keep = ['cal', 'viewers', 'wifi'].filter(x => c.classList.contains(x)).join(' ');
    c.className = 'chip ' + cls + (keep ? ' ' + keep : '');
    c.querySelector('span').textContent = text;
}

function renderMode() {
    const linkOk = S.link === 'ok';
    $$('.p-mode .seg button').forEach(b => {
        const m = b.dataset.mode;
        b.setAttribute('aria-pressed', String(m === S.mode));
        b.classList.toggle('pending', m === S.pending);
        b.setAttribute('aria-disabled', String(!linkOk || (S.cal.active && m !== 'calibrate')));
    });
    if (S.mode) setChip('#chip-mode', 'mode-' + S.mode, MODE_NAMES[S.mode] || S.mode.toUpperCase());
    else setChip('#chip-mode', '', '--');
    const hint = $('#mode-hint');
    hint.className = 'hint';
    if (!linkOk) { hint.textContent = 'Waiting for the turret link.'; hint.classList.add('warn'); }
    else if (S.pending === 'cancel') hint.textContent = 'Cancelling calibration...';
    else if (S.pending) hint.textContent = `Switching to ${MODE_NAMES[S.pending]}...`;
    else if (S.mode === 'calibrate') hint.textContent = 'Calibrating. Jog with the arrows or the joystick.';
    else if (S.mode === 'ai') hint.textContent = 'AI is aiming at the tracked target. Switch to Manual to jog.';
    else if (S.mode === 'user') hint.textContent = 'You are aiming. Hold an arrow, or tap or drag on the video.';
    else hint.textContent = '';
    const can = canJog();
    $('#dpad').classList.toggle('locked', !can);
    $$('.dpad button').forEach(b => b.setAttribute('aria-disabled', String(!can)));
    fireBtn.setAttribute('aria-disabled', String(!linkOk));
    frameEl.classList.toggle('tapmode', can);
}

function renderStatus() {
    const now = performance.now();
    const vs = videoState(now);
    const frozenS = (now - V.lastFrameAt) / 1000;
    // Capped to the widths the chip reserves (data-w, or data-wc on a phone in portrait).
    const compact = COMPACT_BAR.matches;
    if (vs === 'live') setChip('#chip-video', 'ok', `${Math.min(V.fps, 99)} fps`);
    else if (vs === 'frozen') setChip('#chip-video', 'warn', compact ? 'FROZEN' : `FROZEN ${Math.min(frozenS, 99.9).toFixed(1)} s`);
    else if (vs === 'busy') setChip('#chip-video', 'err', 'BUSY');
    else setChip('#chip-video', '', compact ? '...' : 'CONNECTING');
    frameEl.classList.toggle('stale', vs === 'frozen' || vs === 'busy');
    // Top-right corner: fps while live, the frozen timer when frames stop.
    const fpsEl = $('#ov-fps');
    fpsEl.classList.toggle('stale-badge', vs === 'frozen');
    fpsEl.textContent = vs === 'live' ? `${V.fps} fps`
        : vs === 'frozen' ? `FROZEN ${frozenS.toFixed(1)} s` + (frozenS > 5 ? ' · reconnecting' : '')
        : vs === 'busy' ? '0 fps' : 'NO VIDEO';
    $('#card-busy').hidden = vs !== 'busy';
    $('#t-fps').textContent = vs === 'live' ? `${V.fps} fps` : vs === 'frozen' ? 'frozen' : vs === 'busy' ? 'busy' : '--';
    $('#t-dec').textContent = String(V.decodeErrors);

    const vc = $('#chip-viewers');
    if (vs === 'busy') { vc.hidden = false; setChip('#chip-viewers', 'err', '2/2'); }
    else if (S.vw !== null) { vc.hidden = false; setChip('#chip-viewers', S.vw >= 2 ? 'warn' : 'ok', `${S.vw}/2`); }
    else vc.hidden = true;
    vc.title = S.vw >= 2 || vs === 'busy'
        ? 'Both video slots are taken: a third device will get "Video busy"' : 'Video viewers (the board allows 2)';

    const rtt = App.rtt === null ? null : Math.round(App.rtt);
    if (S.link === 'ok') setChip('#chip-link', 'ok', rtt === null ? '--' : `${Math.min(rtt, 9999)} ms`);
    else if (S.link === 'lost') setChip('#chip-link', 'err', 'OFFLINE');
    else setChip('#chip-link', '', compact ? '...' : 'CONNECTING');
    $('#card-link').hidden = S.link !== 'lost' || vs === 'busy';
    $('#t-rtt').textContent = S.link === 'ok' && rtt !== null ? `${rtt} ms` : S.link === 'lost' ? 'offline' : '--';

    const cc = $('#chip-cal');
    cc.hidden = S.calibrated === null;
    if (S.calibrated !== null) setChip('#chip-cal', S.calibrated ? 'ok' : 'warn', S.calibrated ? 'OK' : 'UNCALIBRATED');
    $('#uncal-callout').hidden = S.calibrated !== false || S.cal.active || S.link !== 'ok';
    $('#ov-light').hidden = !(S.lowLight && vs === 'live');
    $('#t-light').textContent = vs === 'live' && S.brightness !== null ? `${Math.round(S.brightness)} / 255` : '--';
}

function renderTelemetry() {
    const t = S.trk;
    const tw = $('#ov-track');
    if (!S.hasReply || S.link !== 'ok') {
        tw.textContent = S.link === 'lost' ? 'NO LINK' : '--';
        tw.className = 'track-word idle';
    } else {
        let word = { locked: 'TRACKING', coasting: 'COASTING', tentative: 'ACQUIRING...', idle: 'SCANNING' }[t.state] || 'SCANNING';
        // Explain the turret moving on its own: in AI it returns home a while after losing the target.
        // The home timer keeps running while a new candidate is only tentative.
        if (t.state === 'idle' && S.ret) word = 'RETURNING HOME';
        else if ((t.state === 'idle' || t.state === 'tentative') && S.home > 0) {
            word = `${word} · HOME IN ${(S.home / 1000).toFixed(1)} s`;
        }
        tw.textContent = word;
        tw.className = 'track-word ' + (t.state in { locked: 1, coasting: 1, tentative: 1 } ? t.state : 'idle');
    }
    const has = S.hasReply && t.state !== 'idle';
    $('#t-target').textContent = S.hasReply ? ({ locked: 'locked', coasting: 'coasting', tentative: 'acquiring' }[t.state] || 'none') : '--';
    $('#t-pos').textContent = has ? `${Math.round(t.x)}, ${Math.round(t.y)}` : '--';
    $('#t-vel').textContent = has ? `${t.vx.toFixed(1)}, ${t.vy.toFixed(1)} px/f` : '--';
    $('#t-area').textContent = has ? `${t.area} px` : '--';
    $('#t-pt').textContent = S.pan !== null && S.tilt !== null ? `${S.pan.toFixed(1)} / ${S.tilt.toFixed(1)}` : '--';
    $('#t-aim').textContent = S.aim ? `${Math.round(S.aim.x)}, ${Math.round(S.aim.y)}` + (S.calibrated === false ? ' ~' : '') : '--';
    const can = canJog();
    $$('.dpad button[data-dir]').forEach(b => b.classList.toggle('at-limit', can && atLimit(b.dataset.dir)));
    const cd = Math.max(0, S.cooldownUntil - performance.now()) / 700;
    fireBtn.style.setProperty('--cd', cd.toFixed(3));
}

function renderAll() { renderMode(); renderWizard(); renderStatus(); renderTelemetry(); }

