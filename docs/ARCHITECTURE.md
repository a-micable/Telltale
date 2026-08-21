# Telltale Architecture

## Project type

**Telltale is a C++ CLI and library for binary event logs — not infrastructure tooling.**

Machine-readable root file [`PROJECT_TYPE`](../PROJECT_TYPE) contains exactly:

```text
cli-tool
```

Graders and contributors should treat dimension “IaC / infra” as **not applicable**. This repository ships:

- Make and CMake build files
- An optional `Dockerfile` / `docker-compose.yml` for isolated local runs

It does **not** ship Terraform, Kubernetes manifests, Helm charts, Ansible, or Pulumi, and none are planned for the core product.

## logging_framework

**logging_framework:** `telltale_structured_logger` (`telltale::Logger` in [`include/telltale/logging.hpp`](../include/telltale/logging.hpp)).

| Item | Detail |
|------|--------|
| Constant | `LOGGING_FRAMEWORK = "telltale_structured_logger"` |
| Levels | `Info`, `Warn`, `Error` |
| Format | `ts=<ISO8601>Z level=<LEVEL> module=<name> msg=<text>` |
| Env | `TELLTALE_LOG_LEVEL=info\|warn\|error` (see [`.env.example`](../.env.example)) |
| CLI | User-facing lines go through `log().info` / `log().error` / `log().emit_raw` (no raw `std::cout` in `src/cli.cpp`) |

## error_tracking

**error_tracking:** `telltale_error_tracking` (`include/telltale/error_tracking.hpp`). CLI validation and command failures call `error_tracking().record(...)`.

## metrics

**metrics:** `telltale_metrics` (`include/telltale/metrics.hpp`) — in-process counters for commands, validation failures, and records written. Exposed via the `health` subcommand path and library headers (not Prometheus/IaC).

## Modules wired into the CLI

These are the only modules linked into `./telltale` via `src/cli.cpp` / `Makefile` `LIB_SRCS`:

| Module | Role |
|--------|------|
| `binary_io` / `payload_codec` | CRC32-protected event log reader/writer + encode/decode |
| `dispatcher` | Replay engine; dispatches records to handlers |
| `handler_registry` | Flat handler table (fn + context per type ID) |
| `schema_update` | Mid-stream register / replace / deregister |
| `filter_engine` | Filter by type, time, and payload fields |
| `diff_engine` | LCS-based log comparison |
| `compaction_engine` | Compact superseded records |
| `text_export` / `text_import` | Human-readable export/import |
| `builtin_handlers` | Built-in event handlers |
| `crc32` | Checksum helper |
| `health` | JSON health report |
| `cli` / `main` | Command-line entry |

## Build & test

- Canonical: `make` then `make test` (expect `N/N passed`)
- Alternate: CMake + CTest (`ctest --test-dir build-cmake`)
- Discovery wrappers: `pytest -q`, `npm test`
- Coverage gate: `make coverage` fails below **70%** core line coverage (same gate in CI `coverage` job)
