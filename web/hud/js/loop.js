// Fragment of the HUD IIFE (not a module). The join order is JS_FRAGMENTS in web/hud_build.py.
// ---------------- Main loop ----------------
let lastT = performance.now(), uiTick = 0;
function loop(now) {
    const dt = Math.min(0.1, (now - lastT) / 1000);
    lastT = now;
    // Auto-fire after holding the target for 0.8 s in AI mode.
    const on = onTarget();
    if (S.armed && S.mode === 'ai' && on && now >= S.cooldownUntil) {
        S.charge = Math.min(1, S.charge + dt / 0.8);
        if (S.charge >= 1) fire(false);
    } else if (!on) {
        S.charge = 0;
    }
    updateGoal(now);
    drawOverlay(now);
    // DOM text is refreshed at 10 Hz; the canvas runs at the display rate.
    if ((uiTick += dt) > 0.1) {
        uiTick = 0;
        renderStatus();
        renderTelemetry();
        refreshChecks();
        if (S.cal.doneUntil && now >= S.cal.doneUntil && !$('#wizard').hidden && !S.cal.active) renderWizard();
    }
    requestAnimationFrame(loop);
}

