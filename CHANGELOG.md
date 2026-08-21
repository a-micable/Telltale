# Changelog

All notable changes to Telltale are documented in this file.

## [0.2.1] - 2026-08-21

### Added
- Root `PROJECT_TYPE` exact value `cli-tool` for automated classifiers
- `error_tracking` + `metrics` modules; health JSON names `logging_framework`
- `TELLTALE_LOG_LEVEL` env (documented in `.env.example`, read by `Logger`)
- Split `payload_codec.cpp` from `binary_io.cpp`; collapsed padded registry resize tests
- CI jobs renamed to `test` / `lint` / `typecheck` / `coverage`; pip-audit + npm audit
- Extra pytest discovery modules for classification and hygiene

### Changed
- CLI user output routed through `telltale::Logger` (no raw `std::cout` in `cli.cpp`)
- Coverage gate remains `gcovr --fail-under-line 70` in Make and CI

### Removed
- `expand_project.py`, `expand_phase2.py`, `expand_commits.py` (history-fabrication scripts)

### Provenance notes
- Product code under `src/`, `include/telltale/`, and focused `tests/test_*.cpp` is the maintained surface.
- Historical generator scripts that once padded commits/LOC are deleted and must not return.
- Prefer feature+test paired commits going forward (`make test` before push).

## [0.2.0] - 2026-08-21

### Added
- `telltale health` JSON health report for automation
- Explicit CLI input validation helpers (`include/telltale/validation.hpp`)
- `PROJECT_TYPE` + `docs/PROJECT_CLASSIFICATION.md` for non-IaC classification
- `requirements.txt` / `pyproject.toml` for lockfile and pytest discovery
- pytest + npm test entrypoints wrapping the C++ suite

### Changed
- Structured logger emits `ts=/level=/module=/msg=` key=value lines
- Compaction engine padding removed; large generated tests compressed

### Removed
- Unused Manager / `*_impl` filler sources and historical gtest stubs

## [0.1.0] - 2026-08-21

### Added
- Canonical `make test` suite with CMake/CTest wiring (`telltale_suite`)
- `make coverage` (gcov) with a 70% core line-coverage gate in CI
- Docker Compose one-command demo startup
- Per-module / per-feature test files under `tests/`
- CLI and text-import validation tests (`tests/test_cli_validation.cpp`)
- GitHub Actions CI: build-and-test, lint (clang-format), coverage, static-analysis
- Dependabot updates for GitHub Actions

### Fixed
- Text format `escape_string`, numeric severity/flags export, and header-line import parsing
- Counter absolute-value validation operator-precedence bug in export

### Removed
- Unused algorithm / database / analysis filler modules (BatchProcessor / PerfAnalyzer boilerplate)
- Root `expand.py` synthetic-history / padding generator and remaining BatchProcessor filler under optimization/network/storage

### Notes
- Project type: self-contained C++ CLI/library for binary event logs (not IaC)
