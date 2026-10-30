"""
@file update_index_html.py
@brief PlatformIO pre-build script: generate index_html.h (built from web/hud/ by hud_build.py)
and wifi_html.h (from web/wifi_setup.html) in ESP32/lib/network/include/.

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
import hud_build  # noqa: E402

# (source name for messages, dst header, renderer, loader returning the page bytes). The HUD is built from web/hud/.
pages = [
    ("web/hud/", "index_html.h", html_header.render_header, lambda: hud_build.build_hud_page(web_dir).encode("utf-8")),
    (
        "wifi_setup.html",
        "wifi_html.h",
        html_header.render_wifi_header,
        lambda: open(os.path.join(web_dir, "wifi_setup.html"), "rb").read(),
    ),
]

for src_name, dst_name, render, load in pages:
    dst_path = os.path.join(project_dir, "lib", "network", "include", dst_name)

    try:
        header = render(load())
    except (OSError, ValueError) as e:
        sys.stderr.write(f"[web-ui] ERROR: cannot build {src_name}: {e}\n")
        env.Exit(1)  # type: ignore  # noqa: F821

    if html_header.write_if_changed(dst_path, header):
        print(f"[web-ui] Generated {os.path.relpath(dst_path, project_dir)}")
    else:
        print(f"[web-ui] {dst_name} up to date")
