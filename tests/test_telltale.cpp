#include <cstring>
#include <string>

#include "test_common.hpp"

static void run_all_suites() {
  run_crc32_tests();
  run_writer_tests();
  run_binary_io_tests();
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
  run_cli_args_tests();
  run_logging_tests();
  run_manager_logging_tests();
  run_fresh_clone_tests();
  run_filler_cleanup_tests();
  run_health_validation_tests();
}

static bool run_selected_suite(const std::string& name) {
  if (name == "all") {
    run_all_suites();
    return true;
  }
  if (name == "crc32") {
    run_crc32_tests();
    return true;
  }
  if (name == "writer") {
    run_writer_tests();
    return true;
  }
  if (name == "binary_io") {
    run_binary_io_tests();
    return true;
  }
  if (name == "filter_engine") {
    run_filter_engine_tests();
    return true;
  }
  if (name == "diff_engine") {
    run_diff_engine_tests();
    return true;
  }
  if (name == "cli_args") {
    run_cli_args_tests();
    return true;
  }
  if (name == "cli_validation") {
    run_cli_validation_tests();
    return true;
  }
  if (name == "logging") {
    run_logging_tests();
    return true;
  }
  if (name == "compaction_engine") {
    run_compaction_engine_tests();
    return true;
  }
  if (name == "text_format") {
    run_text_format_tests();
    return true;
  }
  if (name == "health_validation") {
    run_health_validation_tests();
    return true;
  }
  if (name == "managers" || name == "manager_logging") {
    run_manager_logging_tests();
    return true;
  }
  if (name == "fresh_clone") {
    run_fresh_clone_tests();
    return true;
  }
  if (name == "filler_cleanup") {
    run_filler_cleanup_tests();
    return true;
  }
  if (name == "schema") {
    run_schema_tests();
    return true;
  }
  if (name == "registry") {
    run_registry_tests();
    return true;
  }
  if (name == "endian") {
    run_endian_tests();
    return true;
  }
  if (name == "state_crc") {
    run_state_crc_tests();
    return true;
  }
  if (name == "multi_event") {
    run_multi_event_tests();
    return true;
  }
  if (name == "event_roundtrip") {
    run_event_roundtrip_tests();
    return true;
  }
  if (name == "roundtrip_counter") {
    run_roundtrip_counter_tests();
    return true;
  }
  if (name == "roundtrip_keyvalue") {
    run_roundtrip_keyvalue_tests();
    return true;
  }
  if (name == "roundtrip_print") {
    run_roundtrip_print_tests();
    return true;
  }
  return false;
}

int main(int argc, char* argv[]) {
  std::cout << "Telltale Test Suite" << std::endl;
  std::cout << "===================" << std::endl;

  if (argc >= 2 && std::strcmp(argv[1], "all") != 0) {
    if (!run_selected_suite(argv[1])) {
      std::cerr << "Unknown test suite: " << argv[1] << std::endl;
      return 1;
    }
  } else {
    run_all_suites();
  }

  std::cout << std::endl;
  std::cout << "Results: " << g_tests_passed << "/" << g_tests_run << " passed";
  if (g_tests_failed > 0) std::cout << ", " << g_tests_failed << " FAILED";
  std::cout << std::endl;
  return g_tests_failed > 0 ? 1 : 0;
}
