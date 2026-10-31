// Fragment of the HUD IIFE (not a module). The join order is JS_FRAGMENTS in web/hud_build.py.
// ---------------- /stream video ----------------
// The board pushes one JPEG per binary message; nothing is requested from here.
const videoImg = $('#video');
const V = {
    ws: null, keepalive: null, url: null,
    frames: 0, fps: 0, fpsWindowStart: performance.now(),
    lastFrameAt: 0, openedAt: 0, gotFrame: false, everFrame: false,
    quickCloses: 0, busy: false, reopenNow: false, frozenLogged: false, decodeErrors: 0
};
videoImg.addEventListener('error', () => { V.decodeErrors += 1; });

function connectStream() {
    const proto = location.protocol === 'https:' ? 'wss' : 'ws';
    const ws = new WebSocket(`${proto}://${location.hostname}:${STREAM_PORT}/stream`);
    ws.binaryType = 'blob';
    V.ws = ws;
    V.gotFrame = false;
    V.openedAt = 0;

    ws.onopen = () => {
        V.openedAt = performance.now();
        // Push-only viewers send nothing, so httpd's LRU purge would evict them as idle. A 1-byte
        // message every 2 s refreshes the session (the server drains <=64-byte client frames).
        clearInterval(V.keepalive);
        V.keepalive = setInterval(() => { if (ws.readyState === WebSocket.OPEN) ws.send('k'); }, 2000);
    };

    ws.onmessage = (ev) => {
        if (typeof ev.data === 'string') return;
        const previous = V.url;
        V.url = URL.createObjectURL(ev.data);
        videoImg.src = V.url;
        if (previous) URL.revokeObjectURL(previous);
        const now = performance.now();
        V.frames += 1;
        V.lastFrameAt = now;
        V.gotFrame = true;
        V.everFrame = true;
        V.quickCloses = 0;
        if (V.busy) { V.busy = false; logEvent('Video live', 'ok'); }
    };

    ws.onclose = () => {
        clearInterval(V.keepalive);
        V.keepalive = null;
        V.ws = null;
        // The board streams to 2 viewers at most and closes a third right after the handshake. Three
        // consecutive connections that open and close within 1 s with no frame mean "busy".
        const now = performance.now();
        if (V.openedAt && !V.gotFrame && now - V.openedAt < 1000) V.quickCloses += 1;
        if (V.quickCloses >= 3 && !V.busy) {
            V.busy = true;
            logEvent('Video busy: 2 viewers already', 'err');
        }
        const delay = V.reopenNow ? 0 : V.busy ? 3000 : 1000;
        V.reopenNow = false;
        setTimeout(connectStream, delay);
    };
}

// fps from a 1 s window, computed on a timer so it drops to 0 when frames stop arriving.
setInterval(() => {
    const now = performance.now();
    V.fps = Math.round((V.frames * 1000) / (now - V.fpsWindowStart));
    V.frames = 0;
    V.fpsWindowStart = now;
}, 1000);

// Video health: FROZEN after 1 s without a frame; after 5 s frozen the socket is closed and reopened (just
// past the board's 4 s send timeout; the stall's root cause is on the board; reconnecting is the HUD's part).
function videoState(now) {
    if (V.busy) return 'busy';
    if (!V.everFrame) return 'waiting';
    return now - V.lastFrameAt > 1000 ? 'frozen' : 'live';
}
setInterval(() => {
    const now = performance.now();
    const vs = videoState(now);
    if (vs === 'frozen' && !V.frozenLogged) { V.frozenLogged = true; logEvent('Video frozen', 'warn'); }
    if (vs === 'live' && V.frozenLogged) { V.frozenLogged = false; logEvent('Video live', 'ok'); }
    const ws = V.ws;
    if (ws && ws.readyState === WebSocket.OPEN && now - Math.max(V.lastFrameAt, V.openedAt) > 5000) {
        V.reopenNow = true;
        ws.close();
    }
}, 250);

// Low-light check twice a second: draw the current frame into a tiny canvas and average it. Frames are
// blob URLs, which are same-origin, so getImageData is allowed.
const lightCanvas = document.createElement('canvas');
lightCanvas.width = 32;
lightCanvas.height = 24;
const lightCtx = lightCanvas.getContext('2d', { willReadFrequently: true });
setInterval(() => {
    if (videoState(performance.now()) !== 'live' || !videoImg.naturalWidth) return;
    let d;
    try {
        lightCtx.drawImage(videoImg, 0, 0, 32, 24);
        d = lightCtx.getImageData(0, 0, 32, 24).data;
    } catch (e) {
        return;
    }
    let s = 0;
    for (let i = 0; i < d.length; i += 4) s += d[i] * 0.299 + d[i + 1] * 0.587 + d[i + 2] * 0.114;
    S.brightness = s / (d.length / 4);
    const low = S.brightness < 25;
    if (low && !S.lowLight) logEvent('Low light: tracking may miss targets', 'warn');
    S.lowLight = low;
}, 500);

// ---------------- Snapshot ----------------
function snapshot() {
    const c = document.createElement('canvas');
    c.width = 640;
    c.height = 480;
    const g = c.getContext('2d');
    g.fillStyle = '#000';
    g.fillRect(0, 0, 640, 480);
    g.imageSmoothingEnabled = false;
    if (videoImg.naturalWidth) {
        try { g.drawImage(videoImg, 0, 0, 640, 480); } catch (e) { /* no frame yet */ }
    }
    g.imageSmoothingEnabled = true;
    g.drawImage($('#overlay'), 0, 0, 640, 480);
    const d = new Date(), p = (n) => String(n).padStart(2, '0');
    const name = `void-sentry_${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}_${p(d.getHours())}-${p(d.getMinutes())}-${p(d.getSeconds())}.png`;
    c.toBlob((blob) => {
        if (!blob) return toast('Snapshot failed', 'warn');
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = name;
        document.body.appendChild(a);
        a.click();
        a.remove();
        setTimeout(() => URL.revokeObjectURL(url), 2000);
        toast('Snapshot saved: ' + name, 'ok', 'snap');
        logEvent('Snapshot saved');
    }, 'image/png');
    if (!S.reduced) {
        frameEl.classList.add('shutter');
        setTimeout(() => frameEl.classList.remove('shutter'), 90);
    }
}
$('#btn-snap').onclick = snapshot;

