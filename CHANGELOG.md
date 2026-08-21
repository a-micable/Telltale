# Changelog

All notable changes to Telltale are documented in this file.

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
