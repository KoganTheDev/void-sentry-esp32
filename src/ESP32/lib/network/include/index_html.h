/**
 * @file index_html.h
 * @brief Web Interface for the ESP32-CAM Turret system.
 * This file contains the raw HTML, CSS, and JavaScript stored in Flash memory
 * (PROGMEM). The interface provides a live MJPEG stream, a directional D-Pad
 * for motor control, and a firing trigger.
 */

#pragma once

#include <Arduino.h>

/**
 * @brief Raw string containing the full web interface.
 * Stored in PROGMEM to conserve SRAM. This page is served by the HttpServer
 * index_handler to the client's browser.
 */
static const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <link rel="icon" type="image/png"
        href="data:image/png;base64,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">
    <title>Void Sentry</title>
    <style>
        :root {
            --bg: #0a0a0a;
            --panel: #161616;
            --accent: #ff6b00;
            --text: #e0e0e0;
            --terminal: #00ff41;
            --error: #ff3e3e;
            --border: #333;
        }

        * {
            box-sizing: border-box;
            -webkit-tap-highlight-color: transparent;
        }

        body {
            margin: 0;
            padding: 0;
            background: var(--bg);
            color: var(--text);
            font-family: 'Segoe UI', system-ui, sans-serif;
            height: 100vh;
            height: 100svh;
            overflow: hidden;
            display: flex;
            flex-direction: column;
        }

        .header-brand {
            width: 100%;
            display: flex;
            align-items: center;
            padding: 10px 20px;
            border-bottom: 1px solid var(--accent);
            margin-bottom: 2vh;
            background: linear-gradient(to right, #1a1a1a, #0a0a0a);
        }

        /* --- VOID SENTRY TITLE STYLING --- */
        h1 {
            font-family: 'Orbitron', 'Segoe UI', sans-serif;
            font-size: clamp(1.2rem, 5vw, 1.8rem);
            margin: 0;
            font-weight: 900;
            font-style: italic;
            text-transform: uppercase;
            letter-spacing: 2px;
            background: linear-gradient(180deg, #ffffff 30%, var(--accent) 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            filter: drop-shadow(0 0 5px rgba(255, 107, 0, 0.4));
        }

        .fullscreen-btn {
            margin-left: auto;
            background: none;
            border: none;
            cursor: pointer;
            padding: 5px;
            display: flex;
            align-items: center;
            justify-content: center;
            opacity: 0.7;
            transition: opacity 0.2s;
        }

        .fullscreen-btn:hover {
            opacity: 1;
        }

        .fullscreen-btn svg {
            width: 24px;
            height: 24px;
            fill: var(--accent);
        }

        .dashboard {
            width: 100%;
            flex: 1;
            display: grid;
            grid-template-columns: 1fr 280px;
            grid-template-rows: 1fr auto;
            gap: 10px;
            padding: 10px;
            min-height: 0;
        }

        .logo-icon {
            width: 24px;
            height: 24px;
            border: 3px solid var(--accent);
            border-radius: 50%;
            margin-right: 15px;
            position: relative;
            border-top-color: transparent;
            flex-shrink: 0;
            animation: spin 2s linear infinite;
        }

        @keyframes spin {
            to { transform: rotate(360deg); }
        }

        .logo-icon::after {
            content: '';
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            width: 6px;
            height: 6px;
            background: var(--accent);
            border-radius: 50%;
            box-shadow: 0 0 8px var(--accent);
        }

        .video-box {
            background: #000;
            border-radius: 4px;
            position: relative;
            border: 1px solid var(--border);
            overflow: hidden;
            display: flex;
            align-items: center;
            justify-content: center;
        }

        #streamImg {
            max-width: 100%;
            max-height: 100%;
            object-fit: contain;
        }

        #overlayCanvas {
            position: absolute;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            pointer-events: none;
        }

        .stats-panel {
            background: var(--panel);
            border: 1px solid var(--border);
            border-radius: 4px;
            padding: 15px;
            display: flex;
            flex-direction: column;
            gap: 8px;
        }

        .stats-panel h3 {
            margin: 0 0 5px 0;
            color: var(--accent);
            font-size: 0.8rem;
            text-transform: uppercase;
        }

        .stat-line {
            display: flex;
            justify-content: space-between;
            font-family: 'Courier New', monospace;
            font-size: 0.8rem;
            border-bottom: 1px solid #222;
            padding-bottom: 2px;
        }

        .stat-val {
            color: var(--terminal);
            text-shadow: 0 0 5px rgba(0, 255, 65, 0.3);
        }

        .toggle-btn {
            margin-top: auto;
            padding: 10px;
            background: transparent;
            border: 1px solid var(--accent);
            color: var(--accent);
            cursor: pointer;
            font-weight: bold;
            transition: 0.2s;
        }

        .toggle-btn:active {
            background: var(--accent);
            color: white;
        }

        .control-panel {
            grid-column: 1 / -1;
            background: var(--panel);
            padding: clamp(15px, 4vw, 25px);
            border-radius: 4px;
            display: flex;
            justify-content: center;
            align-items: center;
            flex-wrap: wrap;
            gap: clamp(20px, 8vw, 60px);
            border: 1px solid var(--border);
        }

        .d-pad {
            display: grid;
            grid-template-columns: repeat(3, 50px);
            grid-template-rows: repeat(3, 50px);
            gap: 10px;
        }

        .btn {
            background: #252525;
            border-radius: 4px;
            display: flex;
            align-items: center;
            justify-content: center;
            cursor: pointer;
            border: 1px solid var(--border);
        }

        .btn svg {
            width: 20px;
            fill: white;
            opacity: 0.8;
        }

        .btn:active,
        .btn.active {
            background: var(--accent);
            border-color: var(--accent);
        }

        .up { grid-area: 1 / 2; }
        .left { grid-area: 2 / 1; }
        .right { grid-area: 2 / 3; }
        .down { grid-area: 3 / 2; }

        .fire-btn {
            width: 100%;
            min-width: 200px;
            max-width: 350px;
            padding: 22px;
            font-size: 1.2rem;
            font-weight: bold;
            color: white;
            background: linear-gradient(135deg, #ff4500, #b00);
            border: none;
            border-radius: 4px;
            cursor: pointer;
            box-shadow: 0 4px 0 #600;
            text-transform: uppercase;
        }

        .fire-btn:active {
            transform: translateY(2px);
            box-shadow: 0 2px 0 #600;
        }

        @media (max-width: 800px) {
            .dashboard {
                grid-template-columns: 1fr;
                grid-template-rows: 1fr auto auto;
                overflow-y: auto;
            }
            .video-box {
                aspect-ratio: 16 / 9;
                max-height: 35vh;
            }
        }
    </style>
    <link href="https://fonts.googleapis.com/css2?family=Orbitron:wght@900&display=swap" rel="stylesheet">
</head>

<body>
    <div class="header-brand">
        <div class="logo-icon"></div>
        <h1>Void Sentry</h1>
        <button class="fullscreen-btn" onclick="App.toggleFullScreen()" title="Toggle Fullscreen">
            <svg viewBox="0 0 24 24">
                <path d="M7 14H5v5h5v-2H7v-3zm-2-4h2V7h3V5H5v5zm12 7h-3v2h5v-5h-2v3zM14 5v2h3v3h2V5h-5z"/>
            </svg>
        </button>
    </div>

    <div class="dashboard">
        <main class="video-box">
            <img id="streamImg" src="/stream">
            <canvas id="overlayCanvas"></canvas>
        </main>

        <aside class="stats-panel">
            <h3>Telemetry</h3>
            <div class="stat-line">SYSTEM <span class="stat-val" id="metric-sys">LINKING...</span></div>
            <div class="stat-line">MODE <span class="stat-val" id="metric-mode">AI</span></div>
            <div class="stat-line">TARGET <span class="stat-val" id="metric-lock">CLEAR</span></div>
            <div class="stat-line">COORD <span class="stat-val" id="metric-pos">0,0</span></div>

            <details class="advanced-stats">
                <summary>ADVANCED</summary>
                <div class="stat-line">FPS <span class="stat-val" id="metric-fps">0</span></div>
                <div class="stat-line">CONF <span class="stat-val" id="metric-conf">0%</span></div>
                <div class="stat-line">RAM <span class="stat-val" id="metric-mem">0 KB</span></div>
            </details>

            <button class="toggle-btn" onclick="App.toggleMode()">SWITCH TO MANUAL</button>
        </aside>

        <section class="control-panel">
            <div class="d-pad">
                <div class="btn up" data-dir="up"><svg viewBox="0 0 24 24"><path d="M12 4l-9 10h18z" /></svg></div>
                <div class="btn left" data-dir="left"><svg viewBox="0 0 24 24"><path d="M4 12l10 9v-18z" /></svg></div>
                <div class="btn right" data-dir="right"><svg viewBox="0 0 24 24"><path d="M20 12l-10-9v18z" /></svg></div>
                <div class="btn down" data-dir="down"><svg viewBox="0 0 24 24"><path d="M12 20l9-10h-18z" /></svg></div>
            </div>
            <button class="fire-btn" onmousedown="App.fire(true)" onmouseup="App.fire(false)" ontouchstart="App.fire(true)" ontouchend="App.fire(false)">Engage Fire</button>
        </section>
    </div>

    <script>
        const App = {
            state: {
                socket: null,
                mode: 'AI',
                activeKeys: new Set()
            },

            init() {
                this.canvas = document.getElementById('overlayCanvas');
                this.ctx = this.canvas.getContext('2d');
                this.img = document.getElementById('streamImg');
                this.img.onload = () => this.resize();
                window.onresize = () => this.resize();

                this.setupControls();
                this.setupKeyboard();
                this.connect();
            },

            toggleFullScreen() {
                if (!document.fullscreenElement) {
                    document.documentElement.requestFullscreen().catch(err => {
                        console.error(`Error attempting to enable full-screen mode: ${err.message}`);
                    });
                } else {
                    if (document.exitFullscreen) {
                        document.exitFullscreen();
                    }
                }
            },

            setupKeyboard() {
                const keyMap = {
                    'w': 'up', 'ArrowUp': 'up',
                    'a': 'left', 'ArrowLeft': 'left',
                    's': 'down', 'ArrowDown': 'down',
                    'd': 'right', 'ArrowRight': 'right'
                };

                window.addEventListener('keydown', (e) => {
                    if (e.repeat) return;
                    if (keyMap[e.key]) {
                        const dir = keyMap[e.key];
                        this.state.activeKeys.add(e.key);
                        this.send(`move:${dir}`);
                        document.querySelector(`.${dir}`).classList.add('active');
                    }
                    if (e.code === 'Space') {
                        e.preventDefault();
                        this.fire(true);
                        document.querySelector('.fire-btn').style.transform = "translateY(2px)";
                    }
                });

                window.addEventListener('keyup', (e) => {
                    if (keyMap[e.key]) {
                        this.state.activeKeys.delete(e.key);
                        document.querySelector(`.${keyMap[e.key]}`).classList.remove('active');
                        const remainingMoves = Array.from(this.state.activeKeys).some(k => keyMap[k]);
                        if (!remainingMoves) this.send(`move:stop`);
                    }
                    if (e.code === 'Space') {
                        this.fire(false);
                        document.querySelector('.fire-btn').style.transform = "none";
                    }
                });
            },

            resize() {
                this.canvas.width = this.img.clientWidth;
                this.canvas.height = this.img.clientHeight;
            },

            connect() {
                const url = `ws://${location.host}/ws`;
                this.state.socket = new WebSocket(url);
                this.state.socket.onopen = () => this.updateStatus("ONLINE", "#00ff41");
                this.state.socket.onclose = () => {
                    this.updateStatus("RETRYING", "orange");
                    setTimeout(() => this.connect(), 2000);
                };
                this.state.socket.onmessage = (e) => {
                    try {
                        const data = JSON.parse(e.data);
                        this.handleData(data);
                    } catch (err) {}
                };
            },

            updateStatus(txt, clr) {
                const el = document.getElementById('metric-sys');
                el.textContent = txt; el.style.color = clr;
            },

            handleData(data) {
                document.getElementById('metric-pos').textContent = `${data.x},${data.y}`;
                document.getElementById('metric-fps').textContent = data.fps;
                document.getElementById('metric-lock').textContent = data.lock ? "LOCKED" : "CLEAR";
                document.getElementById('metric-lock').style.color = data.lock ? "var(--error)" : "var(--terminal)";
                this.draw(data);
            },

            send(msg) {
                if (this.state.socket?.readyState === 1) this.state.socket.send(msg);
            },

            setupControls() {
                document.querySelectorAll('.btn').forEach(b => {
                    const d = b.dataset.dir;
                    b.onmousedown = b.ontouchstart = (e) => {
                        e.preventDefault();
                        this.send(`move:${d}`);
                        b.classList.add('active');
                    };
                    b.onmouseup = b.ontouchend = () => {
                        this.send(`move:stop`);
                        b.classList.remove('active');
                    };
                });
            },

            fire(on) {
                this.send(on ? "fire:on" : "fire:off");
            },

            toggleMode() {
                this.state.mode = this.state.mode === 'MANUAL' ? 'AI' : 'MANUAL';
                document.getElementById('metric-mode').textContent = this.state.mode;
                document.querySelector('.toggle-btn').textContent = 
                    this.state.mode === 'MANUAL' ? 'SWITCH TO AI' : 'SWITCH TO MANUAL';
                this.send(`mode:${this.state.mode}`);
            },

            draw(data) {
                this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);
                if (!data.lock) return;
                const x = data.x * (this.canvas.width / 320);
                const y = data.y * (this.canvas.height / 240);
                this.ctx.strokeStyle = '#ff0000';
                this.ctx.lineWidth = 2;
                this.ctx.beginPath();
                this.ctx.arc(x, y, 15, 0, Math.PI * 2);
                this.ctx.moveTo(x - 25, y); this.ctx.lineTo(x + 25, y);
                this.ctx.moveTo(x, y - 25); this.ctx.lineTo(x, y + 25);
                this.ctx.stroke();
            }
        };
        App.init();
    </script>
</body>

</html>
)rawliteral";
