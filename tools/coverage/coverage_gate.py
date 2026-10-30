"""Summarise an lcov tracefile per firmware library and fail when pure-library line or branch coverage drops below a floor.

Usage: coverage_gate.py <coverage.info> [--floor PERCENT] [--branch-floor PERCENT]

Only ESP32/lib/<name>/ sources are counted. The pure libraries (no Arduino/ESP-IDF headers, natively tested) are
gated; any other library that shows up is reported but not gated. Prints a Markdown table, also appended to
$GITHUB_STEP_SUMMARY when set.
"""

import argparse
import os
import re
import sys
from dataclasses import dataclass

PURE_LIBS = (
    "aiming", "checksum", "control", "frames", "hud", "jpeg_gray", "provisioning", "remote", "stats", "tracking"
)
# Pure-library branch coverage was 89.4% (lcov BRDA totals, same file filter as the line gate) when this floor was
# set; the floor is that value minus 2 points, rounded down, so CI stays green but a real drop is caught. Raise it
# when branch coverage improves.
BRANCH_FLOOR_PERCENT = 87.0
_LIB_RE = re.compile(r"ESP32[/\\]lib[/\\]([^/\\]+)[/\\]")


@dataclass
class Counts:
    lines_found: int = 0
    lines_hit: int = 0
    branches_found: int = 0
    branches_hit: int = 0

    def add(self, other: "Counts") -> None:
        self.lines_found += other.lines_found
        self.lines_hit += other.lines_hit
        self.branches_found += other.branches_found
        self.branches_hit += other.branches_hit


def pct(hit: int, found: int) -> float:
    return 100.0 * hit / found if found else 100.0


def parse(text: str) -> dict[str, Counts]:
    """Per-library counts from an lcov tracefile; files outside ESP32/lib/ are skipped."""
    libs: dict[str, Counts] = {}
    lib = None
    record = Counts()
    for raw in text.splitlines():
        line = raw.strip()
        if line.startswith("SF:"):
            match = _LIB_RE.search(line[3:])
            lib = match.group(1) if match else None
            record = Counts()
        elif line.startswith(("LF:", "LH:", "BRF:", "BRH:")):
            key, value = line.split(":", 1)
            field = {"LF": "lines_found", "LH": "lines_hit", "BRF": "branches_found", "BRH": "branches_hit"}[key]
            setattr(record, field, int(value))
        elif line == "end_of_record" and lib is not None:
            libs.setdefault(lib, Counts()).add(record)
            lib = None
    return libs


def report(libs: dict[str, Counts]) -> tuple[str, float]:
    """Markdown table and the combined line coverage of the pure libraries."""
    rows = ["| Library | Lines | Branches | Gated |", "|---|---|---|---|"]
    pure = Counts()
    for name in sorted(libs):
        c = libs[name]
        gated = name in PURE_LIBS
        if gated:
            pure.add(c)
        rows.append(
            f"| {name} | {pct(c.lines_hit, c.lines_found):.1f}% ({c.lines_hit}/{c.lines_found}) "
            f"| {pct(c.branches_hit, c.branches_found):.1f}% | {'yes' if gated else 'no'} |"
        )
    total = pct(pure.lines_hit, pure.lines_found)
    rows.append(f"| **pure libs** | **{total:.1f}%** ({pure.lines_hit}/{pure.lines_found}) "
                f"| {pct(pure.branches_hit, pure.branches_found):.1f}% | |")
    return "\n".join(rows), total


def pure_counts(libs: dict[str, Counts]) -> Counts:
    """Combined counts of the gated (pure) libraries."""
    pure = Counts()
    for name, c in libs.items():
        if name in PURE_LIBS:
            pure.add(c)
    return pure


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("tracefile")
    parser.add_argument("--floor", type=float, default=0.0, help="minimum pure-library line coverage, percent")
    parser.add_argument("--branch-floor", type=float, default=BRANCH_FLOOR_PERCENT,
                        help="minimum pure-library branch coverage, percent (default %(default)s)")
    args = parser.parse_args(argv)

    with open(args.tracefile, encoding="utf-8") as f:
        libs = parse(f.read())
    missing = [name for name in PURE_LIBS if name not in libs]
    table, total = report(libs)
    print(table)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as f:
            f.write("## Native test coverage\n\n" + table + "\n")
    if missing:
        print(f"FAIL: no coverage data for pure libs: {', '.join(missing)}")
        return 1
    if total < args.floor:
        print(f"FAIL: pure-library line coverage {total:.1f}% is below the floor {args.floor:.1f}%")
        return 1
    pure = pure_counts(libs)
    branch_total = pct(pure.branches_hit, pure.branches_found)
    if branch_total < args.branch_floor:
        print(f"FAIL: pure-library branch coverage {branch_total:.1f}% is below the floor {args.branch_floor:.1f}%")
        return 1
    print(f"OK: pure-library line coverage {total:.1f}% (floor {args.floor:.1f}%), "
          f"branch coverage {branch_total:.1f}% (floor {args.branch_floor:.1f}%)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
