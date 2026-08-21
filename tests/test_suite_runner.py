"""Discoverable pytest suite that runs the canonical C++ tests.

Static analyzers that look for pytest/unittest will see this module.
The real assertions live in the C++ harness invoked via scripts/run_tests.sh.
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]


def test_make_test_suite_passes() -> None:
    script = ROOT / "scripts" / "run_tests.sh"
    assert script.is_file(), "scripts/run_tests.sh must exist for fresh clones"
    proc = subprocess.run(
        [str(script)],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
    )
    output = (proc.stdout or "") + (proc.stderr or "")
    sys.stdout.write(output)
    assert proc.returncode == 0, "C++ suite exited non-zero"
    assert "FAILED" not in output
    assert re.search(r"Results:\s+\d+/\d+\s+passed", output), output[-500:]


def test_readme_documents_make_test() -> None:
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    assert "make test" in readme
    assert "scripts/run_tests.sh" in readme
