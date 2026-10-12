"""
@file update_index_html.py
@brief PlatformIO pre-build script: generate ESP32/lib/network/include/index_html.h from web/web_ui.html.

Runs when PlatformIO loads the script, before any source is compiled, so a fresh clone (which has no
generated header) builds. The header is rewritten only when web_ui.html actually changed.
"""

import os
import sys

Import("env")  # type: ignore  # noqa: F821 - provided by PlatformIO/SCons

project_dir = env["PROJECT_DIR"]  # type: ignore  # noqa: F821
web_dir = os.path.normpath(os.path.join(project_dir, "..", "web"))
sys.path.insert(0, web_dir)

import html_header  # noqa: E402

src_path = os.path.join(web_dir, "web_ui.html")
dst_path = os.path.join(project_dir, "lib", "network", "include", "index_html.h")

if not os.path.exists(src_path):
    sys.stderr.write(f"[web-ui] ERROR: {src_path} not found\n")
    env.Exit(1)  # type: ignore  # noqa: F821

with open(src_path, "rb") as f:
    header = html_header.render_header(f.read())

if html_header.write_if_changed(dst_path, header):
    print(f"[web-ui] Generated {os.path.relpath(dst_path, project_dir)}")
else:
    print("[web-ui] index_html.h up to date")
