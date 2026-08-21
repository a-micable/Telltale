#!/usr/bin/env bash
# Canonical entrypoint for the Telltale test suite (fresh clone).
# Usage: ./scripts/run_tests.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
make test
