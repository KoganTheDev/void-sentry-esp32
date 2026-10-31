// Fragment of the HUD IIFE (not a module). The join order is JS_FRAGMENTS in web/hud_build.py.
// ---------------- Feedback: toasts, announcements, haptics, event log ----------------
// One toast per kind: a repeat is ignored until the visible one has gone, so a 25 Hz condition shows one
// toast instead of a pile.
const activeToasts = new Set();
function toast(msg, kind, key) {
    const k = key || msg;
    if (activeToasts.has(k)) return;
    activeToasts.add(k);
    const t = document.createElement('div');
    t.className = 'toast ' + (kind || '');
    t.textContent = msg;
    $('#toasts').appendChild(t);
    setTimeout(() => { t.remove(); activeToasts.delete(k); }, 2600);
}
function announce(msg) { $('#announce').textContent = msg; }

// Phone haptics. Android: the Vibration API. iPhone: Safari has no Vibration API, but toggling a
// switch-style checkbox plays the system haptic (iOS 17.4+), so a hidden one is clicked. Both only work
// inside a tap, so this is called from tap handlers only.
function haptic(pattern) {
    if (typeof navigator.vibrate === 'function') navigator.vibrate(pattern);
    else $('#haptic-label').click();
}

// Event log: newest first, consecutive repeats folded into one row, 50 rows kept.
const LOG = [];
let logOpen = false, logFilter = 'all';
const ICONS = {
    ok: '<path d="M5 12l4 4 10-10"/>',
    warn: '<path d="M12 4l9 16H3z"/><path d="M12 10v4M12 17v.5"/>',
    err: '<circle cx="12" cy="12" r="9"/><path d="M12 7v6M12 16v.5"/>',
    hit: '<circle cx="12" cy="12" r="8"/><circle cx="12" cy="12" r="3"/>',
    info: '<circle cx="12" cy="12" r="2.5"/>'
};
function logEvent(text, kind, cat) {
    kind = kind || 'info';
    const top = LOG[0];
    if (top && top.text === text) { top.count++; top.at = Date.now(); }
    else {
        LOG.unshift({ text, kind, cat: cat || 'system', at: Date.now(), count: 1 });
        if (LOG.length > 50) LOG.length = 50;
    }
    renderLog();
}
function ago(ms) {
    const s = Math.round((Date.now() - ms) / 1000);
    return s < 5 ? 'now' : s < 60 ? `${s} s` : s < 3600 ? `${Math.floor(s / 60)} min` : `${Math.floor(s / 3600)} h`;
}
function renderLog() {
    if (!logOpen) return;
    const ol = $('#log');
    ol.textContent = '';
    const rows = LOG.filter(e => logFilter === 'all' || e.cat === logFilter);
    if (!rows.length) {
        const e = document.createElement('li');
        e.className = 'empty';
        e.textContent = 'Nothing yet.';
        ol.appendChild(e);
        return;
    }
    rows.forEach(e => {
        const li = document.createElement('li');
        li.className = e.kind;
        li.innerHTML = `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2">${ICONS[e.kind]}</svg>`;
        const tx = document.createElement('span');
        tx.textContent = e.text;
        if (e.count > 1) {
            const c = document.createElement('span');
            c.className = 'count';
            c.textContent = `x${e.count}`;
            tx.appendChild(c);
        }
        const tm = document.createElement('time');
        tm.textContent = ago(e.at);
        tm.title = new Date(e.at).toLocaleTimeString();
        li.append(tx, tm);
        ol.appendChild(li);
    });
}
function openLog(on) {
    logOpen = on;
    $('#drawer').hidden = !on;
    $('#btn-log').setAttribute('aria-expanded', String(on));
    renderLog();
}
$('#btn-log').onclick = () => openLog(!logOpen);
$('#log-close').onclick = () => openLog(false);
$('#log-clear').onclick = () => { LOG.length = 0; renderLog(); };
$$('.filters button').forEach(b => b.onclick = () => {
    logFilter = b.dataset.f;
    $$('.filters button').forEach(x => x.setAttribute('aria-pressed', String(x === b)));
    renderLog();
});
setInterval(() => { if (logOpen) renderLog(); }, 5000);

