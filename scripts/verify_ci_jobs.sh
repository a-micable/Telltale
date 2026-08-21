#!/usr/bin/env bash
# Local dry-run equivalent of `act -j dependency-audit --list`:
# assert the CI workflow declares the dependency-audit job.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WF="$ROOT/.github/workflows/ci.yml"
grep -qE '^[[:space:]]*dependency-audit:[[:space:]]*$' "$WF"
echo "OK: dependency-audit job is declared in .github/workflows/ci.yml"
