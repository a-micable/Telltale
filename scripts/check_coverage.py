#!/ usr / bin / env python3
"""Fail if lcov summary line coverage is below a threshold percent."""
from __future__ import annotations

import re
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <coverage.info> <min_percent>", file=sys.stderr)
        return 2
    path = Path(sys.argv[1])
    min_pct = float(sys.argv[2])
    text = path.read_text(errors="replace")
#Prefer overall LF / LH totals from the record stream.
    lines_found = 0
    lines_hit = 0
    for m in re.finditer(r"^LF:(\d+)$", text, re.M):
        lines_found += int(m.group(1))
    for m in re.finditer(r"^LH:(\d+)$", text, re.M):
        lines_hit += int(m.group(1))
    if lines_found == 0:
        print("coverage check failed: no LF entries in report", file=sys.stderr)
        return 1
    pct = 100.0 * lines_hit / lines_found
    print(f"line coverage: {pct:.2f}% ({lines_hit}/{lines_found})")
    if pct < min_pct:
        print(f"coverage check failed: {pct:.2f}% < {min_pct}%", file=sys.stderr)
        return 1
    print(f"coverage check passed: {pct:.2f}% >= {min_pct}%")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
