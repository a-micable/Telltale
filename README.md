# Telltale

Telltale is a binary event log format with a replay engine and plugin-style event handlers. It reads binary event log files and replays them event by event, dispatching each event to a registered handler based on its type ID. Schema update events can register, replace, or deregister handlers mid-stream.

## Features

- Binary writer with CRC32-protected records and finalized header record count
- Binary reader with per-record CRC32 validation
- Flat handler table with function pointer and context per type ID
- Schema update events for dynamic handler registration
- Eight built-in event types with real handlers
- CLI with `write`, `replay`, `verify`, `filter`, `diff`, `compact`, `export`, and `import` subcommands
- Comprehensive test suite (299 tests)

## Quick start (fresh clone)

Telltale has **no third-party package dependencies** and no lockfile to commit. A machine with a C++17 toolchain is enough.

```bash
# Debian/Ubuntu
sudo apt-get update
sudo apt-get install -y build-essential

git clone <repo-url> telltale
cd telltale
make          # builds ./telltale
make test     # builds and runs the suite (expect 299/299 passed)
```

Alternative (CMake + CTest — same suite):

```bash
cmake -S . -B build-cmake
cmake --build build-cmake
ctest --test-dir build-cmake --output-on-failure
```

Or with Docker (no local toolchain required):

```bash
docker compose up --build
```

## Project Type

Telltale is a **self-contained C++ CLI / library** for binary event logs. It is **not** infrastructure-as-code tooling. There is no Terraform, Kubernetes, Helm, Ansible, or Pulumi in this repository **by design**; packaging is Make/CMake plus an optional Docker image for isolated runs. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Build

Requirements: `g++` with C++17 support (`build-essential` on Debian/Ubuntu).

| Command | What it does |
|---------|----------------|
| `make` | Build the `./telltale` binary |
| `make test` | Build and run the test suite (`build/test_telltale`) — **canonical test command** |
| `make coverage` | Rebuild with gcov flags, run tests, emit `coverage/index.html` (fails if core line coverage &lt; 70%) |
| `make clean` | Remove `build/` and `./telltale` |
| `ctest --test-dir build-cmake` | Same suite via CMake (`enable_testing` / CTest) |

Compiler flags: `-Wall -Werror -Wextra -pedantic -std=c++17`

CI runs `make` then `make test` on every push (see `.github/workflows/ci.yml`).

## Testing

The runnable suite lives under `tests/` and is wired through:

- **Make:** `make test` → `./build/test_telltale` (hand-rolled `TEST_ASSERT` / `RUN_TEST` in `tests/test_common.hpp`)
- **CMake/CTest:** target `test_telltale`, test name `telltale_suite`
- **CI:** `.github/workflows/ci.yml` job `build-and-test` executes `make test` and fails if output contains `FAILED`

Expect `Results: 299/299 passed` (count grows when new module tests are added).

## Usage

### write — Generate a sample event log

```bash
./telltale write output.tlog [options]
```

Options:
- `--events N` — Number of events to generate (default: 20)
- `--verbose` — Print progress information

Example:
```bash
./telltale write sample.tlog --events 30 --verbose
```

### replay — Replay a log with handler execution

```bash
./telltale replay input.tlog [options]
```

Options:
- `--verbose` — Print detailed handler output

Example:
```bash
./telltale replay sample.tlog --verbose
```

### verify — Validate CRC32 and schema without executing handlers

```bash
./telltale verify input.tlog [options]
```

Options:
- `--verbose` — Print verification details

Example:
```bash
./telltale verify sample.tlog
```

### filter — Filter records into a new log file

```bash
./telltale filter input.tlog output.tlog [options]
```

Filter matching records by type ID range, timestamp range, or payload field values.

Options:
- `--type ID` — Match exact type ID (hex or decimal)
- `--type-min ID` / `--type-max ID` — Type ID range (inclusive)
- `--time-min MS` / `--time-max MS` — Timestamp range in epoch milliseconds
- `--field NAME OP VALUE` — Payload field filter (`eq`, `ne`, `gt`, `lt`, `contains`, etc.)
- `--combine and|or` — How to combine multiple field filters (default: `and`)
- `--no-schema` — Exclude schema update events
- `--explain` — Print filter statistics

Example:
```bash
./telltale filter sample.tlog filtered.tlog --type 0x0007 --field severity eq 1
```

### diff — Compare two log files

```bash
./telltale diff left.tlog right.tlog [options]
```

Compares two logs record-by-record using LCS-based diff. Reports inserts, deletes, and modifications with field-level detail.

Options:
- `--verbose` — Include unchanged (equal) records in the report

Example:
```bash
./telltale diff before.tlog after.tlog
```

### compact — Merge multiple log files

```bash
./telltale compact output.tlog input1.tlog [input2.tlog ...] [options]
```

Merges two or more log files into one output file. Rewrites the header with the correct record count, optionally deduplicates consecutive reset events, and preserves CRC32 integrity on every output record.

Options:
- `--no-dedupe-reset` — Keep consecutive duplicate reset events
- `--verify-output` — Verify the output file after compaction
- `--verbose` — Print compaction statistics

Example:
```bash
./telltale compact merged.tlog part1.tlog part2.tlog --verify-output
```

### export — Export binary log to plain text

```bash
./telltale export input.tlog output.txt [options]
```

Converts a binary log to a structured plain-text format where each record is human-readable.

Options:
- `--raw-hex` — Include raw hex payload for unknown types
- `--verbose` — Verbose output

Example:
```bash
./telltale export sample.tlog sample.txt
```

### import — Import plain text back to binary

```bash
./telltale import input.txt output.tlog [options]
```

Converts a plain-text log (produced by `export`) back to a valid binary log file.

Options:
- `--lenient` — Continue on parse errors where possible
- `--verbose` — Verbose output

Example:
```bash
./telltale import sample.txt roundtrip.tlog
```

## Filter and Query Engine

The filter engine (`filter_engine.hpp` / `filter_engine.cpp`) loads a log file, builds a timestamp timeline from timestamp events, and iterates over records matching caller-specified criteria:

- **Type ID range** — `--type-min` / `--type-max` or exact `--type`
- **Time range** — `--time-min` / `--time-max` (epoch milliseconds inferred from timestamp events)
- **Payload field filters** — decode each event type's fields and match with comparison operators

Matching records can be written to a new output log file with valid CRC32 checksums.

## Binary Diff Engine

The diff engine (`diff_engine.hpp` / `diff_engine.cpp`) opens two log files and compares them record by record using longest-common-subsequence alignment. It reports:

- **Equal** — identical records in both files
- **Insert** — records present only in the right file
- **Delete** — records present only in the left file
- **Modify** — records at the same alignment position with different payloads, including field-level diffs

## Log Compaction Engine

The compaction engine (`compaction_engine.hpp` / `compaction_engine.cpp`) merges multiple input logs sequentially into one output file:

- Validates CRC32 on all input records (optional)
- Deduplicates consecutive equivalent reset events (optional, default on)
- Writes a new header with the correct final record count
- Recomputes CRC32 on every output record via the standard writer

## Text Export and Import Engine

The text format engine (`text_format.hpp` / `text_format.cpp`) provides round-trip conversion:

**Text format structure:**
```
# Telltale Text Format v1
header_version=1
header_record_count=N
BEGIN_RECORD
  index=0
  type_id=0x1
  type_name=Counter
  crc32=0x...
  name="hits"
  value=42
END_RECORD
```

The exporter decodes each built-in event type into human-readable fields. The importer parses this format and rebuilds valid binary payloads and CRC32 checksums.

## Binary Format Specification

### File Header (14 bytes)

| Offset | Size | Field         | Description                          |
|--------|------|---------------|--------------------------------------|
| 0      | 4    | magic         | ASCII bytes `TLTL`                   |
| 4      | 2    | version       | Format version (currently 1)         |
| 6      | 4    | record_count  | Number of records (written on finalize) |
| 10     | 4    | header_crc    | CRC32 of version + record_count      |

All multi-byte integers are little-endian.

### Event Record

Each record consists of:

| Field          | Size     | Description                              |
|----------------|----------|------------------------------------------|
| type_id        | 2 bytes  | Event type identifier (uint16)           |
| payload_length | 4 bytes  | Length of payload in bytes (uint32)      |
| payload        | N bytes  | Event-specific binary data               |
| crc32          | 4 bytes  | CRC32 of type_id + payload_length + payload |

CRC32 uses the standard IEEE polynomial (same as Ethernet/ZIP).

### Schema Update Event (type 0xFFFF)

Payload structure:

| Offset | Size | Field       | Description                                    |
|--------|------|-------------|------------------------------------------------|
| 0      | 1    | flags       | Operation flags (see below)                      |
| 1      | 2    | type_id     | Target event type to modify                    |
| 3      | 2    | handler_id  | Built-in handler ID (register/replace only)    |

Flags (exactly one must be set):
- `0x01` — Register: install handler for type_id
- `0x02` — Deregister: remove handler for type_id
- `0x04` — Replace: replace existing handler for type_id

## Event Types

| Type ID | Name         | Description                                              |
|---------|--------------|----------------------------------------------------------|
| 0x0001  | Counter      | Increment or set a named counter                         |
| 0x0002  | KeyValue     | Store a string key and integer value                     |
| 0x0003  | Timestamp    | Record a time marker (explicit or current time)          |
| 0x0004  | Checksum     | Verify accumulated state CRC against expected value      |
| 0x0005  | Batch        | Contains sub-events processed inline                     |
| 0x0006  | Reset        | Clear accumulated state (scoped)                         |
| 0x0007  | Print        | Write a formatted string to output                       |
| 0x0008  | Stats        | Print summary of counters, key-values, and timestamps    |
| 0xFFFF  | SchemaUpdate | Register, replace, or deregister handlers mid-stream     |

### Counter Payload (0x0001)

- `uint16` name_length + name bytes
- `uint8` absolute_flag (0=delta, 1=absolute)
- `int64` value

### KeyValue Payload (0x0002)

- `uint16` key_length + key bytes
- `uint8` overwrite_flag
- `int64` value

### Timestamp Payload (0x0003)

- `uint16` label_length + label bytes
- `uint8` explicit_time_flag
- `uint64` epoch_millis (ignored if explicit_time_flag is 0)

### Checksum Payload (0x0004)

- `uint16` label_length + label bytes
- `uint32` expected_crc
- `uint32` scope_flags (0x01=counters, 0x02=key-values, 0x04=timestamps)

### Batch Payload (0x0005)

- `uint32` sub_event_count
- For each sub-event: `uint16` type_id + `uint32` length + payload bytes

### Reset Payload (0x0006)

- `uint8` scope_flags (0xFF=all, 0x01=counters, 0x02=key-values, 0x04=timestamps, 0x08=prints)

### Print Payload (0x0007)

- `uint8` severity (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR)
- `uint16` message_length + message bytes

### Stats Payload (0x0008)

- `uint8` output_flags (0x01=counters, 0x02=key-values, 0x04=timestamps, 0x08=summary)
- `uint16` prefix_length + prefix bytes (optional)

## Handler IDs

| ID     | Handler          |
|--------|------------------|
| 0x0001 | BuiltinCounter   |
| 0x0002 | BuiltinKeyValue  |
| 0x0003 | BuiltinTimestamp |
| 0x0004 | BuiltinChecksum  |
| 0x0005 | BuiltinBatch     |
| 0x0006 | BuiltinReset     |
| 0x0007 | BuiltinPrint     |
| 0x0008 | BuiltinStats     |
| 0x00FF | BuiltinNoOp      |

## Architecture

Modules wired into the CLI (`include/telltale/cli.hpp` / `src/cli.cpp`):

| Module | Role |
|--------|------|
| `binary_io` | Binary event log reader/writer with CRC32-protected records |
| `dispatcher` | Replay engine; dispatches each record to a registered handler |
| `handler_registry` | Flat handler table (function pointer + context per type ID) |
| `schema_update` | Mid-stream register / replace / deregister of handlers |
| `filter_engine` | Filter records by type, time, and payload fields |
| `diff_engine` | LCS-based record and field-level log comparison |
| `compaction_engine` | Compact logs (drop superseded key-values, etc.) |
| `text_format` | Human-readable export/import of binary logs |

Supporting pieces used by those modules: `crc32`, `builtin_handlers`, `types`, `errors`.

## Project Structure

```
include/telltale/
  types.hpp           Shared types, constants, endian helpers
  errors.hpp          Error codes and Result type
  crc32.hpp           CRC32 interface
  binary_io.hpp       Event log reader and writer
  handler_registry.hpp Handler table management
  dispatcher.hpp      Event replay and dispatch
  schema_update.hpp   Schema update encoder/decoder
  filter_engine.hpp   Filter and query engine
  diff_engine.hpp     Binary diff engine
  compaction_engine.hpp Log compaction engine
  text_format.hpp     Text export/import engine
  builtin_handlers.hpp Built-in event handlers and replay state
  cli.hpp             Command-line interface

src/
  crc32.cpp           CRC32 implementation
  binary_io.cpp       Binary reader/writer implementation
  handler_registry.cpp Handler registry implementation
  schema_update.cpp   Schema update codec
  dispatcher.cpp      Dispatcher and replay engine
  filter_engine.cpp   Filter and query engine
  diff_engine.cpp     Binary diff engine
  compaction_engine.cpp Log compaction engine
  text_format.cpp     Text export/import engine
  builtin_handlers.cpp Built-in handlers and replay state
  cli.cpp             CLI implementation
  main.cpp            Entry point

tests/
  test_common.hpp/.cpp Shared TEST_ASSERT / RUN_TEST helpers
  test_binary_io.cpp   Binary I/O and CRC tests
  test_dispatcher.cpp  Dispatcher, schema, and registry tests
  test_filter_engine.cpp Filter engine tests
  test_text_format.cpp Text export/import tests
  test_telltale.cpp    Suite entrypoint
```

## License

This project is provided as-is for demonstration purposes.

## Fuzzing

ClusterFuzzLite build scripts (see build.sh) live under `.clusterfuzzlite/`.

Seed corpus files under `fuzz/corpus/` cover minimal logs, schema updates, double-deregister, and nested batch cache paths. Regenerate with `make corpus`.
