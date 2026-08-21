# Telltale test suite

This directory contains the **runnable** hand-rolled test suite.

## How to run (fresh clone)

```bash
make test
# or
./scripts/run_tests.sh
# or
cmake -S . -B build-cmake && cmake --build build-cmake && ctest --test-dir build-cmake --output-on-failure
```

Expect output ending with `Results: N/N passed`.

## Real suite sources (linked by Makefile / CMake)

| File | Coverage |
|------|----------|
| `test_crc32.cpp` | CRC32 |
| `test_writer.cpp` | writer/reader header & CRC rejection |
| `test_roundtrip_counter.cpp` | counter payload roundtrips |
| `test_roundtrip_keyvalue.cpp` | key/value payload roundtrips |
| `test_roundtrip_print.cpp` | print payload roundtrips |
| `test_endian.cpp` | endian helpers |
| `test_event_roundtrip.cpp` | dispatcher event roundtrips |
| `test_schema.cpp` | schema updates |
| `test_registry.cpp` | handler registry |
| `test_state_crc.cpp` | replay state CRC |
| `test_multi_event.cpp` | multi-event logs |
| `test_filter_engine.cpp` | filter engine + CLI filter |
| `test_diff_engine.cpp` | diff engine + CLI diff |
| `test_compaction_engine.cpp` | compaction engine + CLI compact |
| `test_text_format.cpp` | text export/import |
| `test_cli_validation.cpp` | CLI args / import validation |
| `test_telltale.cpp` | suite entrypoint |
| `test_common.hpp` | `TEST_ASSERT` / `RUN_TEST` harness |

Unrelated historical gtest stubs (not part of this suite) live under `extras/gtest_stubs/` and are **not** built.
