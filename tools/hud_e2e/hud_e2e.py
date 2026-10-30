"""End-to-end HUD checks in a headless Chromium browser (Chrome DevTools Protocol) against mock boards.

usage: python tools/hud_e2e/hud_e2e.py [--browser PATH] [--out DIR] [--base-port 8090] [--debug-port 9334]

Starts tools/hud_mock/mock_board.py ten times (ports base..base+9, with the flags each scenario needs), drives the
HUD in headless Edge or Chrome over raw CDP websockets (no Playwright), prints PASS/FAIL per check and finally
`N/M passed`. Every mock has the PIN 1234 (except the --no-pin one), so scenarios that use the controls log in first;
the login cookie is per host, not per port, so each mock sees the others' cookies as unknown sessions. Exit code 1 if any check failed. Needs the `websockets` and `Pillow` packages (the mock needs Pillow).
"""
import argparse, asyncio, glob, json, os, re, shutil, socket, subprocess, sys, tempfile, time, urllib.request
from pathlib import Path
import websockets

REPO = Path(__file__).resolve().parents[2]
MOCK = str(REPO / "tools" / "hud_mock" / "mock_board.py")
BASE = 8090  # replaced by --base-port in main(); the scenarios address mock N as BASE + N
DL = ""
results = []


def find_browser():
    """Edge or Chrome: the usual Windows install paths first, then the Linux binary names on PATH."""
    if sys.platform == "win32":
        for key in ("PROGRAMFILES(X86)", "PROGRAMFILES", "LOCALAPPDATA"):
            root = os.environ.get(key)
            for rel in ("Microsoft/Edge/Application/msedge.exe", "Google/Chrome/Application/chrome.exe"):
                if root and os.path.exists(os.path.join(root, rel)):
                    return os.path.join(root, rel)
    for name in ("google-chrome", "google-chrome-stable", "chromium", "chromium-browser", "microsoft-edge"):
        found = shutil.which(name)
        if found:
            return found
    return None


def wait_port(port, timeout=20.0):
    """Poll until something accepts connections on 127.0.0.1:port."""
    end = time.time() + timeout
    while time.time() < end:
        try:
            with socket.create_connection(("127.0.0.1", port), 0.3):
                return True
        except OSError:
            time.sleep(0.1)
    return False


def wait_port_free(port, timeout=20.0):
    """Poll until nothing accepts connections on 127.0.0.1:port. On Windows a killed Edge's child processes can keep
    the DevTools port open for a moment; attaching to that dying browser crashed every scenario of the next run."""
    end = time.time() + timeout
    while time.time() < end:
        try:
            with socket.create_connection(("127.0.0.1", port), 0.3):
                time.sleep(0.2)
        except OSError:
            return True
    return False


def url(n, path="/"):
    return f"http://127.0.0.1:{BASE + n}{path}"


def ok(name, cond, detail=""):
    results.append((name, bool(cond), detail))
    print(("PASS " if cond else "FAIL ") + name + (f"  [{detail}]" if detail else ""), flush=True)


def mock(port, *flags):
    return subprocess.Popen([sys.executable, MOCK, "--port", str(BASE + port), *flags],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


class Tab:
    def __init__(self, ws):
        self.ws, self.n = ws, 0

    async def cdp(self, method, **params):
        self.n += 1
        await self.ws.send(json.dumps({"id": self.n, "method": method, "params": params}))
        while True:
            m = json.loads(await self.ws.recv())
            if m.get("id") == self.n:
                if "error" in m:
                    raise RuntimeError(f"{method}: {m['error']}")
                return m.get("result", {})

    async def js(self, expr):
        r = await self.cdp("Runtime.evaluate", expression=expr, returnByValue=True, awaitPromise=True)
        if "exceptionDetails" in r:
            raise RuntimeError(f"js failed: {expr[:80]}: {r['exceptionDetails'].get('text')}")
        return r["result"].get("value")

    async def until(self, expr, timeout=10.0, step=0.2):
        end = time.time() + timeout
        while time.time() < end:
            v = await self.js(expr)
            if v:
                return v
            await asyncio.sleep(step)
        return None

    async def open(self, url, w=1440, h=900):
        await self.cdp("Emulation.setDeviceMetricsOverride", width=w, height=h, deviceScaleFactor=1, mobile=w < 900)
        await self.cdp("Page.navigate", url=url)
        await asyncio.sleep(0.5)
        await self.until("document.readyState === 'complete'")

    async def key(self, key, code, text=None):
        vk = {"Escape": 27, "Slash": 191, "Enter": 13, "Space": 32}.get(code) or ord(code[-1])
        for t in ("keyDown", "keyUp"):
            p = {"type": t, "key": key, "code": code, "windowsVirtualKeyCode": vk, "nativeVirtualKeyCode": vk}
            if text and t == "keyDown":
                p["text"] = text
            await self.cdp("Input.dispatchKeyEvent", **p)

    async def center(self, sel):
        r = await self.js(f"(() => {{ const b = document.querySelector('{sel}').getBoundingClientRect(); return [b.x + b.width / 2, b.y + b.height / 2]; }})()")
        return r

    async def press(self, sel, hold=0.0):
        x, y = await self.center(sel)
        await self.cdp("Input.dispatchMouseEvent", type="mouseMoved", x=x, y=y)
        await self.cdp("Input.dispatchMouseEvent", type="mousePressed", x=x, y=y, button="left", clickCount=1)
        await asyncio.sleep(hold)
        await self.cdp("Input.dispatchMouseEvent", type="mouseReleased", x=x, y=y, button="left", clickCount=1)


TEXT = lambda sel: f"(document.querySelector('{sel}')||{{}}).textContent"
TOASTS = "document.querySelector('#toasts').textContent"
AUTH = TEXT("#chip-auth span")
# Every control is locked: d-pad, mode segment, FIRE, auto-fire, and the login callout is offered.
CONTROLS_LOCKED = ("(() => { const q = (s) => document.querySelector(s);"
                   " return q('#dpad').classList.contains('locked')"
                   " && [...document.querySelectorAll('.p-mode .seg button')].every(b => b.getAttribute('aria-disabled') === 'true')"
                   " && q('#fire').getAttribute('aria-disabled') === 'true' && q('#auto-fire').disabled"
                   " && !q('#login-callout').hidden; })()")


async def submit_pin(tab, pin):
    """Type a PIN into the open login dialog (real key input) and submit it."""
    await tab.js("(() => { const i = document.querySelector('#login-pin'); i.value = ''; i.focus(); })()")
    await tab.cdp("Input.insertText", text=pin)
    await tab.js("document.querySelector('#login-submit').click()")


async def login(tab, pin="1234"):
    """Log in through the HUD (callout button, dialog, PIN) and wait for the board to grant control."""
    if not await tab.until(f"{AUTH} === 'VIEW ONLY' && !document.querySelector('#login-callout').hidden", 8):
        return False
    await tab.js("document.querySelector('#login-go').click()")
    if not await tab.until("document.querySelector('#login-dlg').open", 2):
        return False
    await submit_pin(tab, pin)
    return await tab.until(f"{AUTH} === 'CONTROL' && !document.querySelector('#login-dlg').open", 8)


async def scenario_main(tab):
    """Default mock (calibrated, walking target): help, keys, snapshot, log, fire, auto-fire, calibration."""
    await tab.open(url(0))
    ok("connects: link ok and mode AI", await tab.until(f"/ms/.test({TEXT('#chip-link span')}) && {TEXT('#chip-mode span')} === 'AI'", 8))
    ok("logs in with the PIN", await login(tab))

    await tab.js("document.querySelector('#btn-help').click()")
    ok("help opens", await tab.until("document.querySelector('#help').open", 2))
    await tab.key("Escape", "Escape")
    ok("Esc closes help", await tab.until("!document.querySelector('#help').open", 2))
    await tab.key("?", "Slash", "?")
    ok("? opens help", await tab.until("document.querySelector('#help').open", 2))
    await tab.key("Escape", "Escape")
    await asyncio.sleep(0.3)

    await tab.key("m", "KeyM", "m")
    ok("M switches to MANUAL", await tab.until(f"{TEXT('#chip-mode span')} === 'MANUAL'", 3))
    await tab.press('[data-dir="right"]', hold=1.0)
    # Poll: the telemetry text follows the board's replies, so on a slow runner the pose after release can arrive a
    # reply or two later than the release (a single read here flaked in CI).
    moved = await tab.until(f"!{TEXT('#t-pt')}.trim().startsWith('0.0')", 3)
    ok("hold right arrow jogs the turret", moved, f"pan/tilt {await tab.js(TEXT('#t-pt'))!r}")
    await tab.key("h", "KeyH", "h")
    ok("H recenters (aim point 160,120)", await tab.until(f"/160\\D+120/.test({TEXT('#t-aim')})", 8), await tab.js(TEXT("#t-aim")))
    await tab.key("m", "KeyM", "m")
    ok("M back to AI", await tab.until(f"{TEXT('#chip-mode span')} === 'AI'", 3))

    try:
        await tab.cdp("Browser.setDownloadBehavior", behavior="allow", downloadPath=DL)
    except RuntimeError:  # some Chrome builds reject the browser-level call on a page target
        await tab.cdp("Page.setDownloadBehavior", behavior="allow", downloadPath=DL)
    await tab.js("document.querySelector('#btn-snap').click()")
    end = time.time() + 6
    files = []
    while time.time() < end and not files:
        files = [f for f in os.listdir(DL) if f.endswith(".png")]
        await asyncio.sleep(0.3)
    ok("snapshot downloads void-sentry_YYYY-MM-DD_HH-MM-SS.png",
       files and re.fullmatch(r"void-sentry_\d{4}-\d\d-\d\d_\d\d-\d\d-\d\d\.png", files[0]) and os.path.getsize(os.path.join(DL, files[0])) > 5000,
       f"{files[:1]} {os.path.getsize(os.path.join(DL, files[0])) if files else 0} B")

    # Fire: wait until the reticle is on the locked target (ON TARGET), then fire.
    on = await tab.until("document.querySelector('#fire').classList.contains('ready') || /ON TARGET/.test(document.body.innerText)", 20)
    await tab.press("#fire")
    await asyncio.sleep(0.5)
    await tab.js("document.querySelector('#btn-log').click()")
    await asyncio.sleep(0.3)
    log = await tab.js(TEXT("#log"))
    ok("FIRE on target logs a hit", "Target hit" in (log or ""), f"on_target_seen={bool(on)}")
    await tab.js("document.querySelector('#log-close').click()")

    # Auto-fire: confirm dialog, armed chip, fires on its own.
    await tab.js("document.querySelector('#auto-fire').click()")
    ok("auto-fire asks first (dialog open, not armed)",
       await tab.until("document.querySelector('#arm-dlg').open && document.querySelector('#chip-armed').hidden", 2))
    await tab.js("document.querySelector('#arm-dlg button[value=arm]').click()")
    ok("arming shows the ARMED chip", await tab.until("!document.querySelector('#chip-armed').hidden && document.querySelector('#auto-fire').checked", 2))
    await tab.js("document.querySelector('#log').textContent = ''")
    hits_before = (log or "").count("Target hit")
    await tab.js("document.querySelector('#btn-log').click()")
    fired = await tab.until(f"(document.querySelector('#log').textContent.match(/Target hit/g)||[]).length > {hits_before}", 15)
    ok("auto-fire fires on a held target", fired)
    await tab.js("document.querySelector('#log-close').click()")
    await tab.js("document.querySelector('#auto-fire').click()")
    ok("unticking disarms", await tab.until("document.querySelector('#chip-armed').hidden", 2))

    # Calibration: start, cancel confirm, then a full save.
    await tab.js("document.querySelector('[data-mode=calibrate]').click()")
    ok("CALIBRATE opens the wizard at step 1", await tab.until(f"!document.querySelector('#wizard').hidden && /step 1 of 2/.test({TEXT('#wizard')})", 4))
    await tab.js("document.querySelector('#w-cancel').click()")
    ok("cancel asks to confirm", await tab.until("!!document.querySelector('#w-discard')", 2))
    await tab.js("document.querySelector('#w-discard').click()")
    ok("confirmed cancel returns to AI", await tab.until(f"{TEXT('#chip-mode span')} === 'AI' && document.querySelector('#wizard').hidden", 4))

    await tab.js("document.querySelector('[data-mode=calibrate]').click()")
    await tab.until(f"/step 1 of 2/.test({TEXT('#wizard')})", 4)
    rec1 = await tab.until("(() => { const b = document.querySelector('#w-rec'); return b && b.getAttribute('aria-disabled') === 'false'; })()", 20)
    await tab.js("document.querySelector('#w-rec').click()")
    s2 = await tab.until(f"/step 2 of 2/.test({TEXT('#wizard')})", 4)
    ok("record P1 -> step 2", rec1 and s2, f"rec1={rec1} wizard={(await tab.js(TEXT('#wizard')))!r} toasts={(await tab.js(TOASTS))!r} mode={(await tab.js(TEXT('#chip-mode span')))!r}")
    why = await tab.js(TEXT("#w-why"))
    p1 = [int(v) for v in (await tab.js(TEXT("#t-pos"))).split(",")]
    far = (f"(() => {{ const b = document.querySelector('#w-rec'); const p = document.querySelector('#t-pos').textContent.split(',').map(Number);"
           f" return b && b.getAttribute('aria-disabled') === 'false' && Math.abs(p[0] - {p1[0]}) >= 85 && Math.abs(p[1] - {p1[1]}) >= 65; }})()")
    rec2 = await tab.until(far, 40, 0.1)
    await tab.js("document.querySelector('#w-rec').click()")
    saved = await tab.until(f"/Calibration saved/i.test({TOASTS}) || /step 3|done/.test({TEXT('#wizard')})", 5)
    ok("record P2 far enough -> Calibration saved", rec2 and saved, f"why-before={why!r} rec2={rec2} p1={p1} pos-now={(await tab.js(TEXT('#t-pos')))!r} wizard={(await tab.js(TEXT('#wizard')))!r} toasts={(await tab.js(TOASTS))!r}")
    ok("after save: AI mode, CAL OK", await tab.until(f"{TEXT('#chip-mode span')} === 'AI' && {TEXT('#chip-cal span')} === 'OK'", 6))

    # Too close: the HUD must explain (disabled Record) or the board must fail it; either way it is not saved.
    await asyncio.sleep(3)
    await tab.js("document.querySelector('[data-mode=calibrate]').click()")
    await tab.until(f"/step 1 of 2/.test({TEXT('#wizard')})", 4)
    await tab.until("(() => { const b = document.querySelector('#w-rec'); return b && b.getAttribute('aria-disabled') === 'false'; })()", 20)
    await tab.js("document.querySelector('#w-rec').click()")
    await tab.until(f"/step 2 of 2/.test({TEXT('#wizard')})", 4)
    await asyncio.sleep(0.4)
    state = await tab.js("(() => { const b = document.querySelector('#w-rec'); return {disabled: b && b.getAttribute('aria-disabled') === 'true', why: (document.querySelector('#w-why')||{}).textContent, canvasZone: true}; })()")
    if state and not state["disabled"]:
        await tab.js("document.querySelector('#w-rec').click()")
        failed = await tab.until("!!document.querySelector('#w-err')", 4)
        ok("too-close P2: failure shown with Restart", failed, await tab.js(TEXT("#w-err")))
    else:
        ok("too-close P2: Record disabled with the reason", state and state["why"], state and state["why"])
    await tab.js("document.querySelector('#w-cancel') && document.querySelector('#w-cancel').click()")
    await asyncio.sleep(0.3)
    await tab.js("document.querySelector('#w-discard') && document.querySelector('#w-discard').click()")
    ok("cancel after too-close keeps CAL OK", await tab.until(f"{TEXT('#chip-mode span')} === 'AI' && {TEXT('#chip-cal span')} === 'OK'", 5))


async def scenario_flags(tab):
    await tab.open(url(1))  # --stall-after 2
    ok("stall: VIDEO chip FROZEN and frame greyed",
       await tab.until(f"/FROZEN/.test({TEXT('#chip-video span')}) && document.querySelector('.frame').classList.contains('stale')", 8))
    ok("stall: reconnect brings frames back", await tab.until(f"/fps/.test({TEXT('#chip-video span')})", 10))

    await tab.open(url(2))  # --busy
    ok("busy: 'Video busy' card and BUSY chip",
       await tab.until(f"!document.querySelector('#card-busy').hidden && {TEXT('#chip-video span')} === 'BUSY'", 15))

    await tab.open(url(3))  # --dark
    ok("dark: LOW LIGHT badge", await tab.until("!document.querySelector('#ov-light').hidden", 8))

    await tab.open(url(4))  # --uncalibrated
    await login(tab)
    ok("uncalibrated: callout and CAL UNCALIBRATED",
       await tab.until(f"!document.querySelector('#uncal-callout').hidden && {TEXT('#chip-cal span')} === 'UNCALIBRATED'", 6))
    await tab.js("document.querySelector('#uncal-go').click()")
    ok("callout Calibrate button opens the wizard", await tab.until("!document.querySelector('#wizard').hidden", 4))

    await tab.open(url(5))  # --stick
    await tab.until(f"/ms/.test({TEXT('#chip-link span')})", 6)
    await login(tab)
    await tab.js("document.querySelector('[data-mode=user]').click()")
    await tab.until(f"{TEXT('#chip-mode span')} === 'MANUAL'", 3)
    await tab.press('[data-dir="left"]', hold=1.2)
    ok("stick: 'Joystick has control' toast", await tab.until(f"/Joystick has control/.test({TOASTS})", 2))

    await tab.open(url(6))  # third-viewer refusal: 2 other viewers hold the stream
    ok("third viewer: 'Video busy'", await tab.until(f"!document.querySelector('#card-busy').hidden", 15))

    await tab.open(url(0, "/wifi"))
    ok("WiFi page lists scanned networks", await tab.until("document.body.innerText.length > 200 && document.querySelectorAll('button').length > 2", 6))


async def scenario_auth(tab):
    """Mock 8 (PIN 1234, fresh browser profile): watch without a login, refused PIN, login, control, logout."""
    await tab.open(url(8))
    ok("viewer: video, mode and VIEW ONLY shown",
       await tab.until(f"/fps/.test({TEXT('#chip-video span')}) && {TEXT('#chip-mode span')} === 'AI' && {AUTH} === 'VIEW ONLY'", 10))
    ok("viewer: controls disabled, login offered", await tab.until(CONTROLS_LOCKED, 3))
    await tab.key("m", "KeyM", "m")
    ok("viewer: M is refused with a login hint, mode stays AI",
       await tab.until(f"/log in/i.test({TOASTS}) && {TEXT('#chip-mode span')} === 'AI'", 3))

    await tab.js("document.querySelector('#login-go').click()")
    ok("login dialog opens on the PIN field",
       await tab.until("document.querySelector('#login-dlg').open && document.activeElement === document.querySelector('#login-pin')", 2))
    await submit_pin(tab, "0000")
    ok("wrong PIN shows an error and keeps VIEW ONLY",
       await tab.until(f"/Wrong PIN/.test({TEXT('#login-err')}) && document.querySelector('#login-dlg').open && {AUTH} === 'VIEW ONLY'", 5),
       await tab.js(TEXT("#login-err")))
    await tab.key("Escape", "Escape")
    ok("Esc closes the login dialog", await tab.until("!document.querySelector('#login-dlg').open", 2))

    ok("right PIN unlocks: CONTROL chip, controls enabled", await login(tab)
       and await tab.until("!document.querySelector('#dpad').classList.contains('locked') || "
                           "document.querySelector('.p-mode .seg button').getAttribute('aria-disabled') === 'false'", 3))
    await tab.key("m", "KeyM", "m")
    ok("logged in: M switches to MANUAL", await tab.until(f"{TEXT('#chip-mode span')} === 'MANUAL'", 3))
    before = await tab.js(TEXT("#t-pt"))
    await tab.press('[data-dir="right"]', hold=1.0)
    ok("logged in: jog moves the turret", await tab.until(f"{TEXT('#t-pt')} !== {before!r}", 3),
       f"{before!r} -> {await tab.js(TEXT('#t-pt'))!r}")

    await tab.js("document.querySelector('#chip-auth').click()")
    ok("CONTROL chip opens the log-out dialog", await tab.until("document.querySelector('#logout-dlg').open", 2))
    await tab.js("document.querySelector('#logout-go').click()")
    ok("log out locks the controls again", await tab.until(f"{AUTH} === 'VIEW ONLY' && {CONTROLS_LOCKED}", 5))
    await tab.press('[data-dir="right"]', hold=0.3)
    ok("after log out: a jog is refused with a login hint", await tab.until(f"/log in/i.test({TOASTS})", 3))


async def scenario_no_pin(tab):
    """Mock 9 (--no-pin): control is open, the HUD says so and points to the WiFi page."""
    await tab.open(url(9))
    ok("no PIN: OPEN chip and the set-a-PIN hint",
       await tab.until(f"{AUTH} === 'OPEN' && !document.querySelector('#pin-callout').hidden && document.querySelector('#login-callout').hidden", 8))
    await tab.key("m", "KeyM", "m")
    ok("no PIN: control works without a login", await tab.until(f"{TEXT('#chip-mode span')} === 'MANUAL'", 3))
    await tab.js("document.querySelector('#pin-dismiss').click()")
    ok("no PIN: the hint can be dismissed", await tab.until("document.querySelector('#pin-callout').hidden", 2))


async def scenario_link_lost(tab, proc):
    await tab.open(url(7))
    await tab.until(f"/ms/.test({TEXT('#chip-link span')})", 6)
    proc.kill()
    ok("board gone: OFFLINE chip and 'Turret link lost' card",
       await tab.until(f"{TEXT('#chip-link span')} === 'OFFLINE' && !document.querySelector('#card-link').hidden", 8))


async def hold_two_viewers(port, stop):
    async with websockets.connect(f"ws://127.0.0.1:{port}/stream", max_size=None) as a, \
               websockets.connect(f"ws://127.0.0.1:{port}/stream", max_size=None) as b:
        while not stop.is_set():
            for s in (a, b):
                try:
                    await asyncio.wait_for(s.recv(), 0.05)
                except asyncio.TimeoutError:
                    pass


async def main(args):
    global BASE, DL
    BASE = args.base_port
    out = args.out or tempfile.mkdtemp(prefix="hud_e2e_")
    DL = os.path.join(out, "dl")
    os.makedirs(DL, exist_ok=True)
    for f in glob.glob(os.path.join(DL, "*")):
        os.remove(f)
    browser = args.browser or find_browser()
    if not browser:
        sys.exit("no Chrome/Edge found: pass --browser PATH")
    procs, edge, holder = [], None, None
    stop = asyncio.Event()
    try:
        procs = [mock(0, "--stay"), mock(1, "--stall-after", "2"), mock(2, "--busy"), mock(3, "--dark"),
                 mock(4, "--uncalibrated"), mock(5, "--stick"), mock(6)]
        lost = mock(7)  # killed by scenario_link_lost
        procs += [lost, mock(8, "--stay"), mock(9, "--no-pin")]
        for n in range(10):
            if not wait_port(BASE + n):
                sys.exit(f"mock board on port {BASE + n} did not start")
        if not wait_port_free(args.debug_port):
            sys.exit(f"DevTools port {args.debug_port} is still in use (another browser?); pass --debug-port")
        cmd = [browser, "--headless=new", f"--remote-debugging-port={args.debug_port}", "--no-first-run",
               "--no-default-browser-check", f"--user-data-dir={os.path.join(out, 'profile')}"]
        if sys.platform != "win32":
            cmd += ["--no-sandbox", "--disable-gpu", "--disable-dev-shm-usage"]
        edge = subprocess.Popen(cmd + ["about:blank"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if not wait_port(args.debug_port):
            sys.exit("browser DevTools endpoint did not come up")
        holder = asyncio.create_task(hold_two_viewers(BASE + 6, stop))
        page = None
        for _ in range(100):
            tabs = json.load(urllib.request.urlopen(f"http://127.0.0.1:{args.debug_port}/json"))
            page = next((t for t in tabs if t["type"] == "page"), None)
            if page:
                break
            await asyncio.sleep(0.1)
        async with websockets.connect(page["webSocketDebuggerUrl"], max_size=None) as ws:
            tab = Tab(ws)
            await tab.cdp("Page.enable")
            # scenario_auth first: it needs a profile with no login cookie yet.
            for sc in (scenario_auth, scenario_main, scenario_flags, scenario_no_pin):
                try:
                    await sc(tab)
                except Exception as e:
                    ok(f"{sc.__name__} crashed", False, repr(e))
            try:
                await scenario_link_lost(tab, lost)
            except Exception as e:
                ok("scenario_link_lost crashed", False, repr(e))
    finally:
        stop.set()
        if holder:
            holder.cancel()
        for p in ([edge] if edge else []) + procs:
            p.kill()
            p.wait()
        if edge:
            wait_port_free(args.debug_port, 10.0)  # leave the port free for the next run
    passed = sum(r[1] for r in results)
    print(f"\n{passed}/{len(results)} passed")
    return 0 if results and passed == len(results) else 1


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="End-to-end HUD checks in headless Chrome/Edge against mock boards.")
    ap.add_argument("--browser", help="Chrome/Edge executable (default: autodetect)")
    ap.add_argument("--out", help="scratch dir for the browser profile and downloads (default: a temp dir)")
    ap.add_argument("--base-port", type=int, default=8090, help="first of 10 consecutive mock board ports")
    ap.add_argument("--debug-port", type=int, default=9334, help="browser remote-debugging port")
    sys.exit(asyncio.run(main(ap.parse_args())))
