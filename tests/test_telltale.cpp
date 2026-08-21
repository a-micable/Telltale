#include "test_common.hpp"

int main() {
  std::cout << "Telltale Test Suite" << std::endl;
  std::cout << "===================" << std::endl;
  run_crc32_tests();
  run_writer_tests();
  run_roundtrip_counter_tests();
  run_roundtrip_keyvalue_tests();
  run_roundtrip_print_tests();
  run_endian_tests();
  run_event_roundtrip_tests();
  run_schema_tests();
  run_registry_tests();
  run_state_crc_tests();
  run_multi_event_tests();
  run_filter_engine_tests();
  run_diff_engine_tests();
  run_compaction_engine_tests();
  run_text_format_tests();
  run_cli_validation_tests();
  std::cout << std::endl;
  std::cout << "Results: " << g_tests_passed << "/" << g_tests_run << " passed";
  if (g_tests_failed > 0) std::cout << ", " << g_tests_failed << " FAILED";
  std::cout << std::endl;
  return g_tests_failed > 0 ? 1 : 0;
}
