// Fragment of the HUD IIFE (not a module). The join order is JS_FRAGMENTS in web/hud_build.py.
// The board serves /stream from its own server on port 81 so a stalled video send cannot block
// /commands replies (engineering notes s13). /commands stays on location.host. That split only exists
// when the page came from the default port (80, or 443); a dev server such as the mock board serves
// everything on the one port it was opened on, so stream from there.
const STREAM_PORT = ['', '80', '443'].includes(location.port) ? 81 : location.port;
const $ = (s) => document.querySelector(s);
const $$ = (s) => document.querySelectorAll(s);

// All overlay geometry is in camera pixels (QVGA). The board's tracker, aim pixel and reach box use the
// same space, so only the canvas transform changes with the screen size.
const W = 320, H = 240;
const HOME = { x: 160, y: 120 };
const MODE_NAMES = { ai: 'AI', user: 'MANUAL', calibrate: 'CALIBRATE' };
// Phone portrait: the status bar is too narrow for the long chip states (matches the 700 px CSS block).
const COMPACT_BAR = window.matchMedia('(max-width: 700px)');
const SPEEDS = ['slow', 'normal', 'fast'];
const LIM_BITS = { left: 1, right: 2, down: 4, up: 8 }; // reply `lim`: pan min/max, tilt min/max
const DEFAULT_SLEW_PX_S = 80;
const RET_RED = '#ff2d2d', RET_RGB = '255,45,45', RET_BLACK = 'rgba(0,0,0,0.85)';

const num = (v) => (typeof v === 'number' && Number.isFinite(v) ? v : 0);
const clamp = (v, lo, hi) => Math.min(hi, Math.max(lo, v));
const dist = (a, b) => Math.hypot(a.x - b.x, a.y - b.y);
const easeOutBack = (x) => { const c1 = 1.70158, c3 = c1 + 1; return 1 + c3 * Math.pow(x - 1, 3) + c1 * Math.pow(x - 1, 2); };

// ---------------- State ----------------
// Board values start as null: nothing is shown until the first reply, so the HUD never displays a fake
// mode or pose.
const S = {
    hasReply: false,
    mode: null, calibrated: null, pan: null, tilt: null,
    trk: { state: 'idle', id: 0, x: 0, y: 0, bx: 0, by: 0, bw: 0, bh: 0, vx: 0, vy: 0, area: 0 },
    aim: null, reach: null, lim: 0, home: 0, ret: 0, vw: null,
    auth: null, pinHintDismissed: false,          // reply `auth`: 0 view only, 1 logged in, 2 no PIN (auth.js)
    link: 'connecting', linkSince: performance.now(),
    pending: null, pendingTimer: null,          // 'ai' | 'user' | 'calibrate' | 'cancel'
    counters: null,                              // cal_saves / cal_miss baseline, reset on every connect
    cal: { active: false, step: 1, p1: null, failed: '', confirm: false, doneUntil: 0 },
    held: new Map(), moveDir: null, moveTimer: null,
    speed: 'normal', shift: false,
    uiGoal: null, dragId: null, lastAimSend: 0, lastAimMsg: '', aimTimer: null,
    slew: DEFAULT_SLEW_PX_S, aimSampleT: 0, aimMovedAt: 0,
    armed: false, charge: 0, cooldownUntil: 0, lockSince: 0, acqAt: 0, boxFlashUntil: 0,
    fx: [], trail: [],
    brightness: null, lowLight: false,
    reduced: false
};
const motionQuery = matchMedia('(prefers-reduced-motion: reduce)');
S.reduced = motionQuery.matches;
if (motionQuery.addEventListener) motionQuery.addEventListener('change', (e) => { S.reduced = e.matches; });

