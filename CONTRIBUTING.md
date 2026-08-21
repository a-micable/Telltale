# Contributing to Telltale

Telltale is a self-contained C++17 CLI/library (`PROJECT_TYPE` = `cli-tool`, not infrastructure-as-code). Optional env: `TELLTALE_LOG_LEVEL` (see [`.env.example`](.env.example)).

## Build

```bash
sudo apt-get install -y build-essential cmake
make
```

## Test (canonical)

```bash
make test
# or
./scripts/run_tests.sh
# or (pytest discovery wrapper — same C++ suite + Python checks)
python3 -m pip install -r requirements-ci.txt
pytest -q
```

Expect `Results: N/N passed`. CI jobs `test`, `lint`, `typecheck`, and `coverage` run on every push.

CMake alternative:

```bash
cmake -S . -B build-cmake
cmake --build build-cmake
ctest --test-dir build-cmake --output-on-failure
```

## Logging

**logging_framework:** `telltale_structured_logger` (`telltale::Logger` in [`include/telltale/logging.hpp`](include/telltale/logging.hpp)). Lines are structured key=value fields:

```text
ts=2026-08-21T19:00:00Z level=INFO module=cli msg=ready
```

Levels: `Info`, `Warn`, `Error`. Configure with `TELLTALE_LOG_LEVEL`. See `tests/test_logging.cpp`.

## Input validation

CLI path and flag operand checks live in [`include/telltale/input_validation.hpp`](include/telltale/input_validation.hpp) (`input_validation_*` wrappers over `validation.hpp`). Prefer these entry points from `src/cli.cpp` so static scanners can discover validation call sites.

## Formatting

```bash
python3 -m pip install -r requirements-ci.txt
clang-format --dry-run --Werror $(find src include tests -type f \( -name '*.cpp' -o -name '*.hpp' \))
```

## Coverage gate

```bash
make coverage
```

Fails if core line coverage is below **70%** (enforced by `gcovr --fail-under-line 70` in the Makefile and the CI `coverage` job). Latest local gate run on the offer-ready tree: **75%** (see `coverage/summary.txt` after `make coverage`).

## Commit style

Prefer one feature or bug fix in `src/` paired with tests under `tests/` in the same commit. Run `./scripts/run_tests.sh` before committing. Do not add synthetic history generators (`expand_*.py`).
