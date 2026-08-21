#pragma once

#include <sys/stat.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "telltale/binary_io.hpp"
#include "telltale/builtin_handlers.hpp"
#include "telltale/cli.hpp"
#include "telltale/crc32.hpp"
#include "telltale/dispatcher.hpp"
#include "telltale/filter_engine.hpp"
#include "telltale/handler_registry.hpp"
#include "telltale/schema_update.hpp"
#include "telltale/text_format.hpp"

extern int g_tests_run;
extern int g_tests_passed;
extern int g_tests_failed;
extern std::string g_current_test;

#define TEST_ASSERT(cond)                                                                \
  do {                                                                                   \
    if (!(cond)) {                                                                       \
      std::cerr << "FAIL: " << g_current_test << " - assertion failed: " #cond << " at " \
                << __FILE__ << ":" << __LINE__ << std::endl;                             \
      ++g_tests_failed;                                                                  \
      return false;                                                                      \
    }                                                                                    \
  } while (0)

#define RUN_TEST(fn)                      \
  do {                                    \
    g_current_test = #fn;                 \
    ++g_tests_run;                        \
    std::cout << "  " << #fn << " ... ";  \
    if (fn()) {                           \
      std::cout << "OK" << std::endl;     \
      ++g_tests_passed;                   \
    } else {                              \
      std::cout << "FAILED" << std::endl; \
    }                                     \
  } while (0)

inline bool file_exists(const std::string& path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0;
}

inline void remove_file(const std::string& path) { std::remove(path.c_str()); }

inline std::string temp_path(const std::string& name) {
  return std::string("/tmp/telltale_test_") + name;
}

using namespace telltale;

void run_crc32_tests();
void run_writer_tests();
void run_roundtrip_counter_tests();
void run_roundtrip_keyvalue_tests();
void run_roundtrip_print_tests();
void run_endian_tests();
void run_event_roundtrip_tests();
void run_schema_tests();
void run_registry_tests();
void run_state_crc_tests();
void run_multi_event_tests();
void run_filter_engine_tests();
void run_diff_engine_tests();
void run_compaction_engine_tests();
void run_text_format_tests();
void run_cli_validation_tests();
void run_logging_tests();
void run_manager_logging_tests();
void run_fresh_clone_tests();
