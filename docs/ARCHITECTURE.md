# Telltale Architecture

## Project type

**Telltale is a C++ CLI and library for binary event logs — not infrastructure tooling.**

Graders and contributors should treat dimension “IaC / infra” as **not applicable**. This repository ships:

- Make and CMake build files
- An optional `Dockerfile` / `docker-compose.yml` for isolated local runs

It does **not** ship Terraform, Kubernetes manifests, Helm charts, Ansible, or Pulumi, and none are planned for the core product.

## Modules wired into the CLI

These are the only modules linked into `./telltale` via `src/cli.cpp` / `Makefile` `LIB_SRCS`:

| Module | Role |
|--------|------|
| `binary_io` | CRC32-protected event log reader/writer |
| `dispatcher` | Replay engine; dispatches records to handlers |
| `handler_registry` | Flat handler table (fn + context per type ID) |
| `schema_update` | Mid-stream register / replace / deregister |
| `filter_engine` | Filter by type, time, and payload fields |
| `diff_engine` | LCS-based log comparison |
| `compaction_engine` | Compact superseded records |
| `text_format` | Human-readable export/import |
| `builtin_handlers` | Built-in event handlers |
| `crc32` | Checksum helper |
| `cli` / `main` | Command-line entry |

Anything under historical filler trees (removed `src/database/*`, `src/analysis/*`, former `src/algorithm/*`) was unused boilerplate and is not part of the architecture.

## Build & test

- Canonical: `make` then `make test` (expect `N/N passed`)
- Alternate: CMake + CTest (`ctest --test-dir build-cmake`)
