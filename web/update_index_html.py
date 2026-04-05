Import("env") # type: ignore
def update_index_html(source, target, env):

    import os
    import re


    project_dir = env["PROJECT_DIR"]
    
    src_path = os.path.join(project_dir, "..", "web", "web_ui.html")
    dst_path = os.path.join(project_dir, "lib", "network", "include", "index_html.h")

    try:
        if not os.path.exists(src_path):
            raise FileNotFoundError(f"Source file not found: {src_path}")

        with open(src_path, "r", encoding="utf-8") as f:
            new_html = f.read().strip()

        if not os.path.exists(dst_path):
            raise FileNotFoundError(f"Target file not found: {dst_path}")

        with open(dst_path, "r", encoding="utf-8") as f:
            h_content = f.read()

        pattern = r'(R"rawliteral\()(.*?)(\)rawliteral")'

        new_h_content = re.sub(
            pattern,
            rf'\1\n{new_html}\n\3',
            h_content,
            flags=re.DOTALL
        )

        if h_content == new_h_content:
            print("[Update Raw HTML] No changes detected.")
        else:
            with open(dst_path, "w", encoding="utf-8") as f:
                f.write(new_h_content)
            print("[Update Raw HTML] HTML updated successfully.")

    except Exception as e:
        print(f"Error: {e}")

# Hook script
env.AddPreAction("$BUILD_DIR/${PROGNAME}.elf", update_index_html) # type: ignore