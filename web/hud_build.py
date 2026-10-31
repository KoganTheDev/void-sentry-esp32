"""Join the HUD sources in web/hud/ into the one self-contained page the board serves.

The board serves a single gzipped HTML file (no extra requests, so the offline hotspot works and there is one route).
For people that is one 2,000-line file nobody can review, so the source is split by concern and joined here, at build
time. Pure functions, no PlatformIO dependency: the build hook, the mock board and the tests all call build_hud_page().

Layout of web/hud/:
    index.html   the markup, with two placeholder lines (STYLES_PLACEHOLDER, SCRIPT_PLACEHOLDER)
    css/*.css    the stylesheet, joined in CSS_FILES order (cascade order matters)
    js/*.js      fragments of ONE strict-mode IIFE, joined in JS_FRAGMENTS order (they share one scope)
"""

import os

HUD_DIR_NAME = "hud"

STYLES_PLACEHOLDER = "<!-- @inline styles -->"
SCRIPT_PLACEHOLDER = "<!-- @inline script -->"

# Single source of truth for the join order. Every file in web/hud/css and web/hud/js must be listed here
# (test_web_ui.py checks both directions), so nothing is silently dropped from the page.
CSS_FILES = (
    "base.css",
    "statusbar.css",
    "video.css",
    "panel.css",
    "controls.css",
    "overlays.css",
    "responsive.css",
)

JS_FRAGMENTS = (
    "state.js",
    "feedback.js",
    "link.js",
    "auth.js",
    "controls.js",
    "fire.js",
    "stream.js",
    "calibration.js",
    "status.js",
    "overlay.js",
    "loop.js",
    "ui_events.js",
    "wifi_chip.js",
    "boot.js",
)

FRAGMENT_HEADER_PREFIX = "// Fragment of the HUD IIFE"

# Indentation the joined blocks get inside index.html, so the built page reads like a hand-written one.
_CSS_INDENT = " " * 8
_IIFE_INDENT = " " * 8
_JS_INDENT = " " * 12


def _read(path):
    """Read a source file as text with LF line endings, whatever the checkout's line-ending setting is."""
    with open(path, "rb") as f:
        return f.read().decode("utf-8").replace("\r\n", "\n")


def _indent(text, prefix):
    """Indent every non-blank line; blank lines stay empty."""
    return "\n".join(prefix + line if line else line for line in text.split("\n"))


def _strip_fragment_header(text):
    first, sep, rest = text.partition("\n")
    return rest if first.startswith(FRAGMENT_HEADER_PREFIX) and sep else text


def _replace_placeholder(page, placeholder, block):
    """Replace the line holding only `placeholder` (any indentation) with `block`."""
    lines = page.split("\n")
    hits = [i for i, line in enumerate(lines) if line.strip() == placeholder]
    if len(hits) != 1:
        raise ValueError(f"index.html must hold exactly one '{placeholder}' line, found {len(hits)}")
    lines[hits[0]:hits[0] + 1] = block.split("\n")
    return "\n".join(lines)


def build_hud_page(web_dir):
    """Return the joined HUD page (str, LF line endings, deterministic) from web_dir/hud/."""
    hud = os.path.join(web_dir, HUD_DIR_NAME)
    page = _read(os.path.join(hud, "index.html"))

    css = "".join(_read(os.path.join(hud, "css", name)) for name in CSS_FILES)
    page = _replace_placeholder(page, STYLES_PLACEHOLDER, _indent(css.rstrip("\n"), _CSS_INDENT))

    body = "".join(_strip_fragment_header(_read(os.path.join(hud, "js", name))) for name in JS_FRAGMENTS)
    script = (
        f"{_IIFE_INDENT}(function () {{\n"
        f"{_JS_INDENT}'use strict';\n"
        f"{_indent(body.rstrip(chr(10)), _JS_INDENT)}\n"
        f"{_IIFE_INDENT}}})();"
    )
    return _replace_placeholder(page, SCRIPT_PLACEHOLDER, script)
