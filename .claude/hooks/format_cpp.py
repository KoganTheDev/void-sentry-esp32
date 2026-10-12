"""PostToolUse hook: run clang-format on firmware C/C++ files after Claude edits them.

Only touches ESP32/{src,include,lib,test} files with C/C++ extensions, never the generated
index_html.h. Never blocks an edit: if clang-format is missing or fails, it warns and exits 0.
"""

import json
import os
import shutil
import subprocess
import sys

EXTENSIONS = {".c", ".cpp", ".h", ".hpp"}
FIRMWARE_DIRS = {"src", "include", "lib", "test"}
GENERATED = {"index_html.h"}


def main() -> int:
    try:
        path = json.load(sys.stdin).get("tool_input", {}).get("file_path") or ""
    except Exception:
        return 0
    if not path:
        return 0

    project = os.environ.get("CLAUDE_PROJECT_DIR") or os.getcwd()
    target = os.path.abspath(path)
    try:
        rel = os.path.relpath(target, os.path.join(project, "ESP32"))
    except ValueError:  # different drive on Windows
        return 0

    parts = rel.split(os.sep)
    if parts[0] == ".." or parts[0] not in FIRMWARE_DIRS:
        return 0
    if os.path.splitext(target)[1].lower() not in EXTENSIONS or os.path.basename(target) in GENERATED:
        return 0
    if not os.path.isfile(target):
        return 0

    exe = shutil.which("clang-format")
    if exe is None:
        print("format_cpp hook: clang-format not on PATH, skipped formatting", file=sys.stderr)
        return 0
    result = subprocess.run([exe, "-i", "--style=file", target], cwd=project, capture_output=True, text=True,
                            timeout=30)
    if result.returncode != 0:
        print(f"format_cpp hook: clang-format failed: {result.stderr.strip()}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
