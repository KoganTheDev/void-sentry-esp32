// Fragment of the HUD IIFE (not a module). The join order is JS_FRAGMENTS in web/hud_build.py.
// ---------------- Control access (login) ----------------
// Watching is open; control needs the PIN from the WiFi setup page. The board answers a viewer's messages like any
// other but drops its control commands, and says which case applies in every reply (`auth`). The login cookie is
// HttpOnly, so this page never sees it: it only learns the outcome from `auth`. The board reads the cookie when a
// /commands socket opens, so a login reconnects /commands; a logout needs no reconnect (the board re-checks the
// session on every message). Login and logout are plain fetch() POSTs, never /commands messages.
const AUTH_VIEWER = 0, AUTH_AUTHED = 1, AUTH_OPEN = 2;
const PIN_HINT_KEY = 'vs-pin-hint-dismissed';

// null: no reply yet, or firmware without access control. Only an explicit "view only" locks the controls.
function canControl() { return S.auth !== AUTH_VIEWER; }

// For every control action: true (after a toast) when the user has to log in first.
function needControl() {
    if (canControl()) return false;
    toast('View only: log in to control the turret', 'warn', 'login');
    return true;
}

function readPinHintDismissed() {
    try { return localStorage.getItem(PIN_HINT_KEY) === '1'; } catch (e) { return false; }
}
S.pinHintDismissed = readPinHintDismissed();

function updateAuth(m) {
    if (typeof m.auth !== 'number' || m.auth === S.auth) return;
    const prev = S.auth;
    S.auth = m.auth;
    if (m.auth === AUTH_VIEWER) {
        // Held jogs, aims and a pending mode switch belong to the session that lost control.
        releaseAll();
        clearGoal();
        clearPending();
        disarm('Auto-fire disarmed: view only');
        if (prev !== null) {
            toast('Logged out: view only', 'warn', 'auth');
            logEvent('Logged out: view only', 'warn');
            announce('Logged out. View only');
        }
    } else if (m.auth === AUTH_AUTHED && prev !== null) {
        toast('Logged in: you have control', 'ok', 'auth');
        logEvent('Logged in', 'ok');
        announce('Logged in. You have control');
    }
    renderAll();
}

function renderAuth() {
    const chip = $('#chip-auth');
    chip.hidden = S.auth === null;
    if (S.auth === AUTH_VIEWER) {
        setChip('#chip-auth', 'warn locked', 'VIEW ONLY');
        chip.title = 'View only: log in to control the turret';
    } else if (S.auth === AUTH_AUTHED) {
        setChip('#chip-auth', 'ok', 'CONTROL');
        chip.title = 'You have control: log out';
    } else if (S.auth === AUTH_OPEN) {
        setChip('#chip-auth', 'warn', 'OPEN');
        chip.title = 'No PIN set: anyone on this network can control the turret';
    }
    chip.setAttribute('aria-label', chip.title);
    const linkOk = S.link === 'ok';
    $('#login-callout').hidden = !(linkOk && S.auth === AUTH_VIEWER);
    $('#pin-callout').hidden = !(linkOk && S.auth === AUTH_OPEN && !S.pinHintDismissed);
    $('#auto-fire').disabled = !canControl();
}

// ---- Login dialog ----
const loginDlg = $('#login-dlg'), loginPin = $('#login-pin'), loginErr = $('#login-err'), loginBtn = $('#login-submit');
let loginBusy = false;

function openLogin() {
    if (S.auth === AUTH_AUTHED) return $('#logout-dlg').showModal();
    if (S.auth === AUTH_OPEN) {
        // Nothing to log in with: bring the hint back if it was dismissed.
        S.pinHintDismissed = false;
        renderAuth();
        return toast('No PIN set: control is open. Set a PIN on the WiFi page.', 'warn', 'login');
    }
    setLoginError('');
    loginPin.value = '';
    if (!loginDlg.open) loginDlg.showModal();
    loginPin.focus();
}

function setLoginError(text, busy) {
    loginErr.textContent = text;
    loginErr.classList.toggle('busy', !!busy);
}

// The board's answers: 200 {"ok":true}, 403 {"error":"wrong pin"}, 429 {"locked_s":N} (shared lockout with the
// WiFi page), 400 {"error":"..."} (e.g. no PIN set).
function loginFailure(status, body) {
    if (status === 429 && body && typeof body.locked_s === 'number') return `Too many wrong PINs. Try again in ${body.locked_s} s.`;
    if (status === 403 && body && body.error === 'wrong pin') return 'Wrong PIN.';
    if (body && typeof body.error === 'string' && body.error) return `The turret refused: ${body.error}.`;
    return `Login failed (HTTP ${status}).`;
}

$('#login-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    if (loginBusy) return;
    const pin = loginPin.value;
    if (!/^[0-9]{4,8}$/.test(pin)) {
        setLoginError('The PIN is 4 to 8 digits.');
        loginPin.focus();
        return;
    }
    loginBusy = true;
    loginBtn.setAttribute('aria-disabled', 'true');
    setLoginError('Checking...', true);
    let failure = '';
    try {
        const res = await fetch('/api/hud/login', {
            method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: new URLSearchParams({ pin }).toString(),
            cache: 'no-store'
        });
        let body = null;
        try { body = await res.json(); } catch (err) { body = null; }
        if (res.ok) {
            loginPin.value = '';
            loginDlg.close();
            App.restart(); // the new socket's handshake carries the cookie; its first reply (auth 1) confirms
        } else {
            failure = loginFailure(res.status, body);
        }
    } catch (err) {
        failure = 'Could not reach the turret.';
    }
    loginBusy = false;
    loginBtn.removeAttribute('aria-disabled');
    if (failure) {
        setLoginError(failure);
        logEvent('Login refused: ' + failure, 'warn');
        loginPin.select();
        loginPin.focus();
    }
});
$('#login-cancel').onclick = () => loginDlg.close();
loginDlg.addEventListener('close', () => { loginPin.value = ''; }); // never leave the PIN in the page

async function logout() {
    try {
        await fetch('/api/hud/logout', { method: 'POST', cache: 'no-store' });
    } catch (err) {
        return toast('Could not reach the turret', 'warn', 'auth');
    }
    // The next reply on this socket already says auth 0 (updateAuth reports it).
}
$('#logout-dlg').addEventListener('close', () => {
    if ($('#logout-dlg').returnValue === 'logout') logout();
    $('#logout-dlg').returnValue = '';
});

$('#chip-auth').onclick = openLogin;
$('#login-go').onclick = openLogin;
$('#pin-dismiss').onclick = () => {
    S.pinHintDismissed = true;
    try { localStorage.setItem(PIN_HINT_KEY, '1'); } catch (e) { /* private mode: dismissed for this page only */ }
    renderAuth();
};

