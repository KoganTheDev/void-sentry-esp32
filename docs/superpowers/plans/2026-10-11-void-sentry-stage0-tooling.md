# Void Sentry Stage 0 Tooling Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Finish Stage 0. The web UI header is generated at build time, C/C++ is formatted on edit and only checked in CI, and CI builds both firmware envs and runs the native and generator tests. Then merge `feature/stage0-foundation` into `main` through a reviewed PR.

**Architecture:** `index_html.h` stops being a tracked file. A pure Python module renders it deterministically (gzip with `mtime=0`), and the PlatformIO pre-script calls that module when the script loads, before anything compiles. A Claude Code `PostToolUse` hook runs `clang-format` 19 on edited firmware sources. One GitHub Actions workflow replaces the auto-committing formatter with check-only jobs, alongside the existing cppcheck workflow.

**Tech Stack:** PlatformIO (espressif32 6.12, Arduino core 2.0.17, native), Python 3 (stdlib only), clang-format 19 (WinLibs LLVM 19.1 locally, `pip install "clang-format==19.1.*"` in CI), GitHub Actions, `gh` CLI.

**Spec:** `docs/superpowers/specs/2026-10-11-void-sentry-stage0-design.md`. Read the sections "Formatting", "CI", "Generated web UI header" and "Definition of done".

## Global Constraints

- Language standard is C++17 everywhere. Don't add C++20 features.
- Run PlatformIO from `ESP32/` as `~/.platformio/penv/Scripts/pio.exe`. Never pipe `pio` into a command that exits early (`grep -m1`, `head`), because on Windows pio then hangs. Redirect output to a log file and grep the file instead.
- The native env needs GCC 14. Until the session restarts, prefix commands with `PATH="$(ls -d /c/Users/yuval/AppData/Local/Microsoft/WinGet/Packages/BrechtSanders.WinLibs.POSIX.UCRT.LLVM*/mingw64/bin | head -1):$PATH"`. The same prefix puts `clang-format` 19 on PATH.
- Never read, edit, copy or print `ESP32/include/secrets.h`. A hook blocks any shell command containing the substring `secret`. To write a file that mentions it, such as a workflow, use the Write tool. To write a commit message that mentions it, put the message in a file and run `git commit -F <file>`.
- Never use em dashes in any text: code comments, docs, commit messages. Use " - " instead.
- Every commit message ends with the line `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Work on branch `feature/stage0-foundation`. Only Task 5 pushes.
- Formatting rules come from the repo's `.clang-format` (LLVM base, Allman braces, 4-space indent, 120 columns, left pointer alignment). Don't change `.clang-format`.

## Review Focus

1. **Fresh clone with no `index_html.h`:** the firmware build must still succeed, so the header has to be generated before compilation, not at link time. Pinned by Task 1, Step 7, which deletes the header and builds.
2. **Rebuilding with an unchanged `web_ui.html`:** the header file must not be rewritten (same bytes, same modification time), so git and incremental builds stay quiet. Pinned by the `test_identical_content_is_not_rewritten` and `test_output_is_deterministic` tests in Task 1.
3. **Editing a file the hook must not touch:** for example `web/web_ui.html`, a Python script, `ESP32/platformio.ini`, a file outside the repo, or the generated `index_html.h`. The hook must exit 0 and leave the file byte-identical. Pinned by Task 2, Step 2.
4. **`clang-format` not on PATH:** the edit must still succeed. The hook warns on stderr and exits 0 rather than blocking. Pinned by Task 2, Step 2 (payload `no_tool`).
5. **CI on a clean runner:** there's no `secrets.h` and no `index_html.h`. The build job must create `secrets.h` from the template, and the format check must not scan generated files. Pinned by Task 4's workflow steps and checked live in Task 5.

---

## File structure

| File | Status | Responsibility |
|---|---|---|
| `web/html_header.py` | Create | Pure functions: render the C header text from HTML bytes; write a file only if its content changed |
| `web/test_html_header.py` | Create | stdlib `unittest` tests for `html_header.py` |
| `web/update_index_html.py` | Rewrite | PlatformIO `pre:` script: locate the files, call `html_header`, generate at load time |
| `.gitignore` | Modify | Ignore the generated `ESP32/lib/network/include/index_html.h` (an entry already exists; the file just needs untracking) |
| `.claude/hooks/format_cpp.py` | Create | `PostToolUse` hook: `clang-format -i` on edited firmware C/C++ files |
| `.claude/settings.json` | Modify | Register the `PostToolUse` hook |
| `.github/workflows/ci.yml` | Create | Jobs: format check, generator tests, firmware builds, native tests |
| `.github/workflows/cpp_lint_and_format.yml` | Delete | Replaced by `ci.yml` |
| `CLAUDE.md` | Modify | Keep the "Web UI pipeline" and formatting notes accurate |

---

### Task 1: Generate `index_html.h` at build time (model: Sonnet)

**Files:**
- Create: `web/html_header.py`
- Create: `web/test_html_header.py`
- Rewrite: `web/update_index_html.py`
- Untrack: `ESP32/lib/network/include/index_html.h`
- Modify: `CLAUDE.md` (section "## Web UI pipeline")

**Interfaces:**
- Consumes: nothing from other tasks. The firmware uses `HTML_PAGE_GZ` (array of `uint8_t`) and `HTML_PAGE_GZ_LEN` (`size_t`) in `ESP32/lib/network/src/turret_server.cpp:94`. Those names and types must not change.
- Produces: `html_header.render_header(html: bytes) -> str` and `html_header.write_if_changed(path: str, content: str) -> bool`. CI (Task 4) runs `python -m unittest discover -s web -p "test_*.py"`.

- [ ] **Step 1: Write the failing tests**

Create `web/test_html_header.py`:

```python
"""Tests for html_header: run with `python -m unittest discover -s web -p "test_*.py"` from the repo root."""

import gzip
import os
import re
import tempfile
import unittest

import html_header

SAMPLE = b"<!doctype html><html><body><h1>Void Sentry</h1></body></html>"


def _array_bytes(header: str) -> bytes:
    body = re.search(r"HTML_PAGE_GZ\[\] PROGMEM = \{(.*?)\};", header, re.S).group(1)
    return bytes(int(v) for v in body.replace("\n", " ").split(",") if v.strip())


class RenderHeaderTests(unittest.TestCase):
    def test_array_decompresses_to_input(self):
        header = html_header.render_header(SAMPLE)
        self.assertEqual(gzip.decompress(_array_bytes(header)), SAMPLE)

    def test_length_constant_matches_array(self):
        header = html_header.render_header(SAMPLE)
        length = int(re.search(r"HTML_PAGE_GZ_LEN = (\d+);", header).group(1))
        self.assertEqual(length, len(_array_bytes(header)))

    def test_output_is_deterministic(self):
        # gzip normally embeds the current time; the header must not change between builds
        self.assertEqual(html_header.render_header(SAMPLE), html_header.render_header(SAMPLE))

    def test_header_keeps_firmware_contract(self):
        header = html_header.render_header(SAMPLE)
        self.assertIn("#pragma once", header)
        self.assertIn("#include <Arduino.h>", header)
        self.assertIn("static const size_t HTML_PAGE_GZ_LEN", header)
        self.assertIn("static const uint8_t HTML_PAGE_GZ[] PROGMEM", header)
        self.assertIn("generated", header.lower())


class WriteIfChangedTests(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.path = os.path.join(self.dir.name, "index_html.h")

    def tearDown(self):
        self.dir.cleanup()

    def test_creates_missing_file(self):
        self.assertTrue(html_header.write_if_changed(self.path, "a"))
        with open(self.path, encoding="utf-8") as f:
            self.assertEqual(f.read(), "a")

    def test_identical_content_is_not_rewritten(self):
        html_header.write_if_changed(self.path, "a")
        os.utime(self.path, (1_000_000, 1_000_000))
        self.assertFalse(html_header.write_if_changed(self.path, "a"))
        self.assertEqual(os.path.getmtime(self.path), 1_000_000)

    def test_changed_content_is_rewritten(self):
        html_header.write_if_changed(self.path, "a")
        self.assertTrue(html_header.write_if_changed(self.path, "b"))
        with open(self.path, encoding="utf-8") as f:
            self.assertEqual(f.read(), "b")


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the tests to verify they fail**

Run from the repo root: `python -m unittest discover -s web -p "test_*.py"`
Expected: an error, `ModuleNotFoundError: No module named 'html_header'`.

- [ ] **Step 3: Write `web/html_header.py`**

```python
"""Render the firmware's gzipped web UI header from web_ui.html.

Pure functions, no PlatformIO dependency, so they can be unit tested and reused by the build hook.
"""

import gzip
import os

_BYTES_PER_LINE = 20

_TEMPLATE = """\
/**
 * @file index_html.h
 * @brief Compressed web interface for the Void Sentry turret.
 * @details GENERATED by web/update_index_html.py from web/web_ui.html on every build - do not edit.
 * Gzip-compressed HTML/CSS/JS stored in flash (PROGMEM) and served with 'Content-Encoding: gzip'.
 */

#pragma once

#include <Arduino.h>

/** @brief Length of the gzip-compressed page in bytes (sent as Content-Length). */
static const size_t HTML_PAGE_GZ_LEN = {length};

/** @brief Gzip-compressed web interface, stored in flash to save SRAM. */
static const uint8_t HTML_PAGE_GZ[] PROGMEM = {{
{data}
}};
"""


def render_header(html: bytes) -> str:
    """Return the full C header for the given HTML. Deterministic: same input, same output."""
    compressed = gzip.compress(html, compresslevel=9, mtime=0)
    lines = []
    for start in range(0, len(compressed), _BYTES_PER_LINE):
        chunk = compressed[start : start + _BYTES_PER_LINE]
        lines.append(", ".join(str(b) for b in chunk))
    return _TEMPLATE.format(length=len(compressed), data=",\n".join(lines))


def write_if_changed(path: str, content: str) -> bool:
    """Write content to path only if it differs. Returns True if the file was written."""
    if os.path.exists(path):
        with open(path, encoding="utf-8") as f:
            if f.read() == content:
                return False
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(content)
    return True
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `python -m unittest discover -s web -p "test_*.py"`
Expected: `Ran 7 tests ... OK`

- [ ] **Step 5: Rewrite `web/update_index_html.py`**

Replace the whole file with the following. It now generates **when the script loads**: PlatformIO runs `pre:` scripts before building, and the old `AddPreAction` on the `.elf` fired only at link time, which is too late once the header is no longer committed.

```python
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
```

- [ ] **Step 6: Untrack the generated header**

Run from the repo root:

```bash
git update-index --no-assume-unchanged ESP32/lib/network/include/index_html.h
git rm --cached -q ESP32/lib/network/include/index_html.h
git check-ignore -v ESP32/lib/network/include/index_html.h
```

Expected: the last command prints a `.gitignore` line that matches the path. The ignore entry already exists at `.gitignore:241`. If `check-ignore` prints nothing, add the line `ESP32/lib/network/include/index_html.h` to `.gitignore`.

- [ ] **Step 7: Verify a fresh-clone build (Review Focus 1 and 2)**

```bash
cd ESP32
rm lib/network/include/index_html.h
~/.platformio/penv/Scripts/pio.exe run -e freenove_esp32_wrover > ../build1.log 2>&1; echo "exit=$?"
grep -E "\[web-ui\]|^freenove_esp32_wrover +(SUCCESS|FAILED)" ../build1.log
~/.platformio/penv/Scripts/pio.exe run -e freenove_esp32_wrover > ../build2.log 2>&1; echo "exit=$?"
grep -E "\[web-ui\]|^freenove_esp32_wrover +(SUCCESS|FAILED)" ../build2.log
rm ../build1.log ../build2.log
cd .. && git status --short
```

Expected:
- First build: `exit=0`, then `[web-ui] Generated lib\network\include\index_html.h` and `SUCCESS`.
- Second build: `exit=0`, then `[web-ui] index_html.h up to date` and `SUCCESS`.
- `git status` does not list `index_html.h`.

- [ ] **Step 8: Update CLAUDE.md**

In `CLAUDE.md`, replace the entire `## Web UI pipeline` section (its three bullets) with:

```markdown
## Web UI pipeline

- Edit `web/web_ui.html` only. `ESP32/lib/network/include/index_html.h` is **generated and gitignored**: `web/update_index_html.py` (a PlatformIO `pre:` script) runs before compilation and writes it via `web/html_header.py`, only when the content changed. Gzip uses `mtime=0` so the output is deterministic.
- `python -m unittest discover -s web -p "test_*.py"` tests the generator (also run in CI).
- `web/compress_HTML_to_C_header.py` / `decompress_C_header_to_HTML.py` are standalone manual converters, not part of the build.
```

- [ ] **Step 9: Commit**

```bash
git add web/html_header.py web/test_html_header.py web/update_index_html.py CLAUDE.md .gitignore
git commit -m "Generate index_html.h at build time instead of tracking it" -m "The header is rendered deterministically (gzip mtime=0) by web/html_header.py before compilation and is now gitignored, so builds no longer leave the tree dirty. Adds unit tests for the generator.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

(`git rm --cached` from Step 6 is already staged and goes into this commit.)

---

### Task 2: Format-on-edit hook (model: Haiku)

**Files:**
- Create: `.claude/hooks/format_cpp.py`
- Modify: `.claude/settings.json`

**Interfaces:**
- Consumes: the hook's stdin JSON (`{"tool_input": {"file_path": "..."}}`) and the env var `CLAUDE_PROJECT_DIR`.
- Produces: nothing other tasks call. Task 3 reformats the existing files once.

- [ ] **Step 1: Create `.claude/hooks/format_cpp.py`**

```python
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
```

- [ ] **Step 2: Pipe-test it (Review Focus 3 and 4)**

From the repo root, with the WinLibs PATH prefix from Global Constraints:

```bash
export CLAUDE_PROJECT_DIR="$PWD"
H=.claude/hooks/format_cpp.py
printf 'int  main( ){return 0;}\n' > ESP32/src/zz_hooktest.cpp
echo '{"tool_input":{"file_path":"ESP32/src/zz_hooktest.cpp"}}' | python -I $H; echo "exit=$?"
cat ESP32/src/zz_hooktest.cpp
cp web/web_ui.html /tmp/ui_before.html
echo '{"tool_input":{"file_path":"web/web_ui.html"}}' | python -I $H; echo "exit=$?"; cmp web/web_ui.html /tmp/ui_before.html && echo "web_ui unchanged"
echo '{"tool_input":{"file_path":"ESP32/platformio.ini"}}' | python -I $H; echo "exit=$?"
echo '{"tool_input":{"file_path":"C:/Windows/win.ini"}}' | python -I $H; echo "exit=$?"
echo '{"tool_input":{"file_path":"ESP32/lib/network/include/index_html.h"}}' | python -I $H; echo "exit=$?"
echo '{"tool_input":{"file_path":"ESP32/src/zz_hooktest.cpp"}}' | PATH=/usr/bin python -I $H; echo "no_tool exit=$?"
rm ESP32/src/zz_hooktest.cpp
```

Expected:
- First call: `exit=0`, and the file now reads (Allman braces):
  ```
  int main()
  {
      return 0;
  }
  ```
- `web_ui unchanged`, and `exit=0` for every other call.
- The `no_tool` call prints `format_cpp hook: clang-format not on PATH, skipped formatting` and `no_tool exit=0`. If `python` isn't found under `PATH=/usr/bin`, use the absolute path from `which python` instead.

- [ ] **Step 3: Register the hook in `.claude/settings.json`**

Add a `PostToolUse` entry next to the existing `PreToolUse` one. The resulting file:

```json
{
  "permissions": {
    "deny": [
      "Read(**/secrets.h)",
      "Edit(**/secrets.h)"
    ]
  },
  "hooks": {
    "PreToolUse": [
      {
        "matcher": "Bash|PowerShell",
        "hooks": [
          {
            "type": "command",
            "command": "python \"$CLAUDE_PROJECT_DIR/.claude/hooks/guard_wifi_credentials.py\"",
            "timeout": 10
          }
        ]
      }
    ],
    "PostToolUse": [
      {
        "matcher": "Write|Edit",
        "hooks": [
          {
            "type": "command",
            "command": "python \"$CLAUDE_PROJECT_DIR/.claude/hooks/format_cpp.py\"",
            "timeout": 30
          }
        ]
      }
    ]
  }
}
```

Validate: `python -I -c "import json; json.load(open('.claude/settings.json')); print('ok')"` → `ok`.

- [ ] **Step 4: Commit**

```bash
git add .claude/hooks/format_cpp.py .claude/settings.json
git commit -m "Add clang-format on-edit hook for firmware sources" -m "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: One-time reformat (model: Haiku)

**Files:**
- Modify: every `*.c`, `*.cpp`, `*.h`, `*.hpp` under `ESP32/src`, `ESP32/include`, `ESP32/lib`, `ESP32/test`, except the generated `index_html.h`.

**Interfaces:**
- Consumes: Task 1, because `index_html.h` must already be untracked so it isn't reformatted or committed.
- Produces: a tree that passes `clang-format --dry-run --Werror`. Task 4's CI check relies on this.

- [ ] **Step 1: Confirm the formatter version**

Run (with the WinLibs PATH prefix): `clang-format --version`
Expected: `clang-format version 19.1.x`. Stop and report if it's any other major version.

- [ ] **Step 2: Reformat**

```bash
find ESP32/src ESP32/include ESP32/lib ESP32/test -type f \( -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) ! -name index_html.h -print0 | xargs -0 clang-format -i --style=file
git status --short
```

Expected: a list of modified source files. `index_html.h` must not appear.

- [ ] **Step 3: Verify the check passes and nothing broke**

```bash
find ESP32/src ESP32/include ESP32/lib ESP32/test -type f \( -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) ! -name index_html.h -print0 | xargs -0 clang-format --dry-run --Werror --style=file; echo "format check exit=$?"
cd ESP32
~/.platformio/penv/Scripts/pio.exe run -e freenove_esp32_wrover > ../b.log 2>&1; grep -E "^freenove_esp32_wrover +(SUCCESS|FAILED)" ../b.log
~/.platformio/penv/Scripts/pio.exe run -e pin_check > ../b.log 2>&1; grep -E "^pin_check +(SUCCESS|FAILED)" ../b.log
~/.platformio/penv/Scripts/pio.exe test -e native > ../b.log 2>&1; grep -E "test cases" ../b.log
rm ../b.log; cd ..
```

Expected: `format check exit=0`, both builds `SUCCESS`, and `9 test cases: 9 succeeded`.

- [ ] **Step 4: Commit**

```bash
git add ESP32/src ESP32/include ESP32/lib ESP32/test
git commit -m "Apply clang-format 19 to firmware sources" -m "Formatting only, no code changes. Establishes the baseline for the CI format check.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: CI workflow (model: Sonnet)

**Files:**
- Create: `.github/workflows/ci.yml` (write it with the Write tool, because it mentions the secrets template)
- Delete: `.github/workflows/cpp_lint_and_format.yml`
- Modify: `CLAUDE.md` (the "Formatting:" bullet under "## Commands")

**Interfaces:**
- Consumes: the generator tests (Task 1), the formatted tree (Task 3), the envs `freenove_esp32_wrover`, `pin_check` and `native` from `ESP32/platformio.ini`, and `ESP32/include/secrets.h.example`.
- Produces: GitHub check names `format`, `web-ui-generator`, `build` and `native-tests`. Task 5 waits on them.

- [ ] **Step 1: Create `.github/workflows/ci.yml`**

```yaml
# Builds, tests and format-checks the firmware. Never rewrites the branch:
# format locally (the Claude Code hook does it on edit) and CI only verifies.
name: CI

on:
    push:
        branches: [main]
    pull_request:

jobs:
    format:
        runs-on: ubuntu-latest
        steps:
            - uses: actions/checkout@v6
            - uses: actions/setup-python@v5
              with:
                  python-version: "3.12"
            - name: Install clang-format 19 (same major version as local LLVM 19)
              run: pip install "clang-format==19.1.*"
            - name: Check formatting
              run: |
                  clang-format --version
                  find ESP32/src ESP32/include ESP32/lib ESP32/test -type f \
                      \( -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \) \
                      ! -name index_html.h -print0 \
                      | xargs -0 clang-format --dry-run --Werror --style=file

    web-ui-generator:
        runs-on: ubuntu-latest
        steps:
            - uses: actions/checkout@v6
            - uses: actions/setup-python@v5
              with:
                  python-version: "3.12"
            - name: Test index_html.h generator
              run: python -m unittest discover -s web -p "test_*.py" -v

    build:
        runs-on: ubuntu-latest
        steps:
            - uses: actions/checkout@v6
            - uses: actions/setup-python@v5
              with:
                  python-version: "3.12"
            - uses: actions/cache@v4
              with:
                  path: ~/.platformio
                  key: pio-${{ runner.os }}-${{ hashFiles('ESP32/platformio.ini') }}
            - name: Install PlatformIO
              run: pip install -U platformio
            - name: Use placeholder WiFi credentials
              run: cp ESP32/include/secrets.h.example ESP32/include/secrets.h
            - name: Build turret firmware
              working-directory: ESP32
              run: pio run -e freenove_esp32_wrover
            - name: Build pin_check
              working-directory: ESP32
              run: pio run -e pin_check

    native-tests:
        runs-on: ubuntu-latest
        steps:
            - uses: actions/checkout@v6
            - uses: actions/setup-python@v5
              with:
                  python-version: "3.12"
            - uses: actions/cache@v4
              with:
                  path: ~/.platformio
                  key: pio-native-${{ runner.os }}-${{ hashFiles('ESP32/platformio.ini') }}
            - name: Install PlatformIO
              run: pip install -U platformio
            - name: Run host unit tests
              working-directory: ESP32
              run: pio test -e native
```

- [ ] **Step 2: Delete the auto-committing workflow**

```bash
git rm -q .github/workflows/cpp_lint_and_format.yml
```

- [ ] **Step 3: Validate the YAML locally**

```bash
~/.platformio/penv/Scripts/python.exe -I -c "import yaml,sys; d=yaml.safe_load(open('.github/workflows/ci.yml')); print(sorted(d['jobs']))" 2>&1 || python -I -c "print('pyyaml unavailable - rely on Task 5')"
```

Expected: `['build', 'format', 'native-tests', 'web-ui-generator']`, or the fallback message. The real validation happens in Task 5.

- [ ] **Step 4: Update CLAUDE.md**

Under `## Commands`, replace the bullet that starts with `- Formatting:` with:

```markdown
- Formatting: `.clang-format` (LLVM base, Allman braces for functions/classes/control statements, 4-space indent, 120 columns, `int* p`), clang-format **19** locally (WinLibs LLVM) and in CI (`pip install "clang-format==19.1.*"`). A Claude Code `PostToolUse` hook (`.claude/hooks/format_cpp.py`) formats firmware C/C++ files on edit. CI (`ci.yml`) only checks with `--dry-run --Werror` and never commits to the branch.
- CI (`ci.yml`): `format`, `web-ui-generator`, `build` (both firmware envs, with `secrets.h` copied from the example) and `native-tests`. cppcheck stays in `static_analysis.yml`.
```

- [ ] **Step 5: Commit**

Write the message to a scratch file with the Write tool (it mentions the secrets template), then:

```bash
git add .github/workflows/ci.yml CLAUDE.md
git commit -F <scratch-file>
```

Message:

```
Replace auto-committing formatter with check-only CI

New ci.yml checks clang-format 19 formatting, tests the web UI header
generator, builds both firmware envs (secrets.h copied from the example)
and runs the native unit tests. The old workflow rewrote branches with
"auto-formmated code" commits; formatting now happens locally on edit.

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
```

---

### Task 5: Push, PR, green CI (main session, not delegated)

**Files:** none, unless CI finds a problem, in which case fix it in a new commit on the branch.

**Interfaces:**
- Consumes: the check names `format`, `web-ui-generator`, `build` and `native-tests` (Task 4), plus `Cppcheck Static Analysis`.
- Produces: an open PR with all checks green, ready for the merge gate.

- [ ] **Step 1: Final local verification**

Run, from `ESP32/` with the WinLibs PATH prefix: both firmware builds, `pio test -e native`, the generator tests (`python -m unittest discover -s ../web -p "test_*.py"`) and the format check from Task 3, Step 3.
Expected: all pass.

- [ ] **Step 2: Push and open the PR**

```bash
git push -u origin feature/stage0-foundation
gh pr create --base main --head feature/stage0-foundation --title "Stage 0: verified pin map, C++17, native tests, check-only CI" --body-file <scratch-file>
```

The PR body summarises the spec's definition of done, links the spec and plan, and ends with:

```
🤖 Generated with [Claude Code](https://claude.com/claude-code)
```

- [ ] **Step 3: Wait for checks**

Run: `gh pr checks --watch`
Expected: `format`, `web-ui-generator`, `build`, `native-tests` and `Cppcheck Static Analysis` all pass. If one fails, read its log with `gh run view --log-failed`, fix it locally, commit, push, and watch again.

- [ ] **Step 4: Merge gate (user decision)**

Don't merge automatically. Show the user the PR link and check results, then use the `merge` skill: summary of commits first, then a separate question about deleting the branch.
