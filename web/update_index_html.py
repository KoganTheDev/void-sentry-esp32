"""
@file update_index_html.py
@brief PlatformIO pre-build script: generate index_html.h (from web/web_ui.html) and wifi_html.h (from web/wifi_setup.html)
in ESP32/lib/network/include/.

Runs when PlatformIO loads the script, before any source is compiled, so a fresh clone (which has no
generated header) builds. Each header is rewritten only when its source actually changed.
"""

import os
import sys

Import("env")  # type: ignore  # noqa: F821 - provided by PlatformIO/SCons

project_dir = env["PROJECT_DIR"]  # type: ignore  # noqa: F821
web_dir = os.path.normpath(os.path.join(project_dir, "..", "web"))
sys.path.insert(0, web_dir)

import html_header  # noqa: E402

pages = [
    ("web_ui.html", "index_html.h", html_header.render_header),
    ("wifi_setup.html", "wifi_html.h", html_header.render_wifi_header),
]

for src_name, dst_name, render in pages:
    src_path = os.path.join(web_dir, src_name)
    dst_path = os.path.join(project_dir, "lib", "network", "include", dst_name)

    if not os.path.exists(src_path):
        sys.stderr.write(f"[web-ui] ERROR: {src_path} not found\n")
        env.Exit(1)  # type: ignore  # noqa: F821

    with open(src_path, "rb") as f:
        header = render(f.read())

    if html_header.write_if_changed(dst_path, header):
        print(f"[web-ui] Generated {os.path.relpath(dst_path, project_dir)}")
    else:
        print(f"[web-ui] {dst_name} up to date")
