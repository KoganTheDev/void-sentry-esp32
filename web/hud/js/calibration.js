// Fragment of the HUD IIFE (not a module). The join order is JS_FRAGMENTS in web/hud_build.py.
// ---------------- Calibration wizard ----------------
function renderWizard() {
    const w = $('#wizard');
    const c = S.cal;
    const done = performance.now() < c.doneUntil;
    w.hidden = !(c.active || done);
    if (w.hidden) return;
    const step = done ? 3 : c.step;
    const stepCls = (n) => step > n ? 'step done' : step === n ? 'step cur' : 'step';
    let body = '';
    if (step === 3) {
        body = '<p class="instr" style="color:var(--ok)">&#10003; Saved to the turret. Switching to AI tracking...</p>';
    } else if (c.confirm) {
        body = `<div class="confirm"><p>Discard this calibration?${S.calibrated ? ' The previous calibration is kept.' : ''}</p>
            <div class="row"><button class="btn-sm danger" id="w-discard">Discard</button>
            <button class="btn-sm" id="w-keep">Keep calibrating</button></div></div>`;
    } else if (c.failed) {
        body = `<div class="err-box" id="w-err"></div>
            <div class="row"><button class="btn-sm primary" id="w-rec">Restart</button>
            <button class="btn-sm" id="w-cancel">Cancel</button></div>`;
    } else {
        const instr = step === 1
            ? 'Get the target brackets to lock on a spot near one corner (wave there). Tap it or jog until the reticle sits on it, then record.'
            : 'Now a spot near the <b>opposite</b> corner, outside the shaded zone. Lock, aim, record.';
        body = `<p class="instr">${instr}</p>
            <ul class="checks">
                <li id="ck-target">Target locked</li>
                <li id="ck-still">Turret still</li>
            </ul>
            <div class="row"><button class="btn-sm primary" id="w-rec">Record point</button>
            <button class="btn-sm" id="w-cancel">Cancel</button></div>
            <p class="hint warn" id="w-why"></p>`;
    }
    w.innerHTML = `<h3>Calibration <span>${step === 3 ? 'done' : 'step ' + step + ' of 2'}</span></h3>
        <div class="steps"><div class="${stepCls(1)}">1 &middot; First corner</div><div class="${stepCls(2)}">2 &middot; Opposite corner</div><div class="${step === 3 ? 'step done' : 'step'}">3 &middot; Saved</div></div>${body}`;
    const err = w.querySelector('#w-err');
    if (err) err.textContent = c.failed; // board text: never parsed as HTML
    const on = (id, fn) => { const el = w.querySelector(id); if (el) el.onclick = fn; };
    on('#w-rec', record);
    on('#w-cancel', askCancel);
    on('#w-discard', cancelCal);
    on('#w-keep', () => { c.confirm = false; renderWizard(); });
    if (c.confirm) { const k = w.querySelector('#w-keep'); if (k) k.focus(); }
    refreshChecks();
}
// Cheap live refresh of the checklist without rebuilding the buttons.
function refreshChecks() {
    const ck = $('#ck-target'), st = $('#ck-still'), rec = $('#w-rec'), why = $('#w-why');
    if (!ck || !st || !rec || !why) return;
    ck.className = trackUsable() ? 'yes' : '';
    st.className = turretStill() ? 'yes' : '';
    const r = recordBlock();
    rec.setAttribute('aria-disabled', String(!!r));
    rec.title = r || 'Record point';
    why.textContent = r;
}

