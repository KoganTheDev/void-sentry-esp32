"""PreToolUse hook: block shell commands that mention secrets.h (WiFi credentials).

Read/Edit deny rules in settings.json cover the file tools; this covers Bash/PowerShell.
Exit code 2 blocks the tool call and shows the message to Claude.

This is a guard against accidents, not a security boundary: it matches text, so wildcards that
avoid the word (e.g. `cat include/*.h`), recursive greps, or a filename built at runtime get past it.
The real fix is removing secrets.h (on-device WiFi provisioning, planned for Stage 1).
The check is the substring "secret", so it also catches globs like secret* and the .example template.
Keep this file's own name free of that word, or commands touching the hook get blocked too.
"""
import json
import sys

try:
    command = json.load(sys.stdin).get("tool_input", {}).get("command", "")
except Exception:
    sys.exit(0)

if "secret" in command.lower():
    print(
        "Blocked: secrets.h holds the user's WiFi credentials. Claude must not read, copy or modify it - "
        "ask the user to do it themselves.",
        file=sys.stderr,
    )
    sys.exit(2)
