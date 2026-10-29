Web UI sources for the turret: `web_ui.html` (HUD) and `wifi_setup.html` (WiFi provisioning) are embedded into the firmware by `update_index_html.py` (generates the gitignored `index_html.h`, helpers in `html_header.py`).
Also here: `test_web_ui.py` and `test_html_header.py` (generator tests), and the standalone converters `compress_HTML_to_C_header.py` / `decompress_C_header_to_HTML.py`.
