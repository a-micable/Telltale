#include "test_common.hpp"

int main() {
  std::cout << "Telltale Test Suite" << std::endl;
  std::cout << "===================" << std::endl;
  run_binary_io_tests();
  run_dispatcher_tests();
  run_filter_engine_tests();
  run_text_format_tests();
  std::cout << std::endl;
  std::cout << "Results: " << g_tests_passed << "/" << g_tests_run << " passed";
  if (g_tests_failed > 0) std::cout << ", " << g_tests_failed << " FAILED";
  std::cout << std::endl;
  return g_tests_failed > 0 ? 1 : 0;
}
