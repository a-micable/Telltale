cmake_minimum_required(VERSION 3.15)
project(telltale VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TELLTALE_CXX_FLAGS -Wall -Werror -Wextra -pedantic)

set(TELLTALE_LIB_SOURCES
  src/crc32.cpp
  src/binary_io.cpp
  src/handler_registry.cpp
  src/schema_update.cpp
  src/dispatcher.cpp
  src/builtin_handlers.cpp
  src/filter_engine.cpp
  src/diff_engine.cpp
  src/compaction_engine.cpp
  src/text_format.cpp
  src/cli.cpp
)

add_library(telltale STATIC ${TELLTALE_LIB_SOURCES})
target_include_directories(telltale PUBLIC include)
target_compile_options(telltale PRIVATE ${TELLTALE_CXX_FLAGS})

add_executable(telltale-cli src/main.cpp)
target_link_libraries(telltale-cli PRIVATE telltale)
target_compile_options(telltale-cli PRIVATE ${TELLTALE_CXX_FLAGS})
set_target_properties(telltale-cli PROPERTIES OUTPUT_NAME telltale)

# Hand-rolled suite (same sources as `make test`). No third-party test framework.
set(TELLTALE_TEST_SOURCES
  tests/test_common.cpp
  tests/test_binary_io.cpp
  tests/test_dispatcher.cpp
  tests/test_filter_engine.cpp
  tests/test_text_format.cpp
  tests/test_telltale.cpp
)

add_executable(test_telltale ${TELLTALE_TEST_SOURCES})
target_include_directories(test_telltale PRIVATE include tests)
target_link_libraries(test_telltale PRIVATE telltale)
target_compile_options(test_telltale PRIVATE ${TELLTALE_CXX_FLAGS})

enable_testing()
include(CTest)
add_test(NAME telltale_suite COMMAND test_telltale)
set_tests_properties(telltale_suite PROPERTIES
  PASS_REGULAR_EXPRESSION "[0-9]+/[0-9]+ passed"
  FAIL_REGULAR_EXPRESSION "FAILED"
)
