"""
@file update_web_ui.py
@brief PlatformIO Build Hook for Automated Web UI Compression.

This script runs automatically before the ELF binary is linked. It performs 
the following workflow:
1. Locates the source 'web_ui.html' file outside the firmware source tree.
2. Compresses the HTML/CSS/JS content using Gzip to minimize Flash usage.
3. Generates a C++ compatible byte array and updates 'index_html.h'.
4. Injects the new byte array and updated length constant using Regex.

This ensures that any changes made to the Web UI are reflected in the 
firmware without manual hex-conversion or copy-pasting.
"""

Import("env")  # type: ignore

import os
import re
import gzip


def update_index_html(source, target, env):
    project_dir = env["PROJECT_DIR"]
    src_path = os.path.join(project_dir, "..", "web", "web_ui.html")
    dst_path = os.path.join(project_dir, "lib", "network", "include", "index_html.h")

    try:
        if not os.path.exists(src_path):
            return

        with open(src_path, "rb") as f:
            html_bytes = f.read()

        compressed = gzip.compress(html_bytes)
        byte_array = ", ".join(str(b) for b in compressed)

        with open(dst_path, "r", encoding="utf-8") as f:
            h_content = f.read()

        # Update GZIP HTML length
        len_pattern = r'(static const size_t HTML_PAGE_GZ_LEN\s*=\s*)\d+;'
        new_h_content = re.sub(len_pattern, rf'\g<1>{len(compressed)};', h_content)

        # Update GZIP HTML content
        data_pattern = r'(static const uint8_t HTML_PAGE_GZ\[\] PROGMEM\s*=\s*\{)[^}]*(\};)'
        new_h_content = re.sub(data_pattern, rf'\g<1>\n{byte_array}\n\g<2>', new_h_content, flags=re.DOTALL)

        if h_content == new_h_content:
            print("[Update HTML] No changes detected.")
        else:
            with open(dst_path, "w", encoding="utf-8") as f:
                f.write(new_h_content)
            print(f"[Update HTML] Success! Compressed to {len(compressed)} bytes.")

    except Exception as e:
        print(f"Error: {e}")

# Hook script
env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", update_index_html)  # type: ignore