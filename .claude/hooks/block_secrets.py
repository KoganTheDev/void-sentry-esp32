"""PreToolUse hook: block shell commands that mention secrets.h (WiFi credentials).

Read/Edit deny rules in settings.json cover the file tools; this covers Bash/PowerShell.
Exit code 2 blocks the tool call and shows the message to Claude.
"""
import json
import sys

try:
    command = json.load(sys.stdin).get("tool_input", {}).get("command", "")
except Exception:
    sys.exit(0)

if "secrets.h" in command.lower():
    print(
        "Blocked: secrets.h holds the user's WiFi credentials. Claude must not read, copy or modify it - "
        "ask the user to do it themselves.",
        file=sys.stderr,
    )
    sys.exit(2)
