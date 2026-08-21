# Contributing to Telltale

Telltale is a self-contained C++17 CLI/library (not infrastructure-as-code). There are no runtime secrets and no required environment variables (see [`.env.example`](.env.example)).

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
# or (pytest discovery wrapper — same C++ suite)
python3 -m pip install -r requirements-ci.txt
pytest -q
```

Expect `Results: N/N passed`. CI runs Make, pytest, and CTest on every push.

CMake alternative:

```bash
cmake -S . -B build-cmake
cmake --build build-cmake
ctest --test-dir build-cmake --output-on-failure
```

## Logging

Use `telltale::Logger` from [`include/telltale/logging.hpp`](include/telltale/logging.hpp). Lines are structured key=value fields:

```text
ts=2026-08-21T19:00:00Z level=INFO module=cli msg=ready
```

Levels: `Info`, `Warn`, `Error`. See `tests/test_logging.cpp`.

## Formatting

```bash
python3 -m pip install -r requirements-ci.txt
clang-format --dry-run --Werror $(find src include tests -type f \( -name '*.cpp' -o -name '*.hpp' \))
```

## Coverage gate

```bash
make coverage
```

Fails if core line coverage is below **70%** (same gate as `.github/workflows/ci.yml`).

## Commit style

Prefer one feature or bug fix in `src/` paired with tests under `tests/` in the same commit. Run `./scripts/run_tests.sh` before committing.
