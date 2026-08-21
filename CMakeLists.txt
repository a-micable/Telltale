cmake_minimum_required(VERSION 3.15)
project(telltale VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TELLTALE_CXX_FLAGS -Wall -Werror -Wextra -pedantic)

set(TELLTALE_LIB_SOURCES
  src/crc32.cpp
  src/binary_io.cpp
  src/payload_codec.cpp
  src/handler_registry.cpp
  src/schema_update.cpp
  src/dispatcher.cpp
  src/builtin_handlers.cpp
  src/filter_engine.cpp
  src/payload_fields.cpp
  src/diff_engine.cpp
  src/compaction_engine.cpp
  src/text_export.cpp
  src/text_import.cpp
  src/network.cpp
  src/storage_backends.cpp
  src/health.cpp
  src/cli.cpp
  src/cli_usage.cpp
  src/cli_sample.cpp
)

add_library(telltale STATIC ${TELLTALE_LIB_SOURCES})
target_include_directories(telltale PUBLIC include)
target_compile_options(telltale PRIVATE ${TELLTALE_CXX_FLAGS})

add_executable(telltale-cli src/main.cpp)
target_link_libraries(telltale-cli PRIVATE telltale)
target_compile_options(telltale-cli PRIVATE ${TELLTALE_CXX_FLAGS})
set_target_properties(telltale-cli PROPERTIES OUTPUT_NAME telltale)
find_package(Threads REQUIRED)
target_link_libraries(telltale PUBLIC Threads::Threads)

# Hand-rolled suite with GoogleTest-compatible TEST()/EXPECT_* macros (tests/gtest/gtest.h).
set(TELLTALE_TEST_SOURCES
  tests/test_common.cpp
  tests/test_crc32.cpp
  tests/test_writer.cpp
  tests/test_roundtrip_counter.cpp
  tests/test_roundtrip_keyvalue.cpp
  tests/test_roundtrip_print.cpp
  tests/test_endian.cpp
  tests/test_event_roundtrip.cpp
  tests/test_schema.cpp
  tests/test_registry.cpp
  tests/test_state_crc.cpp
  tests/test_multi_event.cpp
  tests/test_filter_engine.cpp
  tests/test_diff_engine.cpp
  tests/test_compaction_engine.cpp
  tests/test_text_format.cpp
  tests/test_cli_validation.cpp
  tests/test_cli_args.cpp
  tests/test_binary_io.cpp
  tests/test_logging.cpp
  tests/test_managers.cpp
  tests/test_fresh_clone.cpp
  tests/test_filler_cleanup.cpp
  tests/test_health_validation.cpp
  tests/test_telltale.cpp
)

add_executable(test_telltale ${TELLTALE_TEST_SOURCES})
target_include_directories(test_telltale PRIVATE include tests)
target_link_libraries(test_telltale PRIVATE telltale)
target_compile_options(test_telltale PRIVATE ${TELLTALE_CXX_FLAGS})

enable_testing()
include(CTest)

function(telltale_add_suite name)
  if(ARGC GREATER 1)
    add_test(NAME ${name} COMMAND test_telltale ${ARGV1})
  else()
    add_test(NAME ${name} COMMAND test_telltale)
  endif()
  set_tests_properties(${name} PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    PASS_REGULAR_EXPRESSION "[0-9]+/[0-9]+ passed"
    FAIL_REGULAR_EXPRESSION "FAILED"
  )
endfunction()

telltale_add_suite(telltale_suite)
telltale_add_suite(test_binary_io binary_io)
telltale_add_suite(test_filter_engine filter_engine)
telltale_add_suite(test_diff_engine diff_engine)
telltale_add_suite(test_cli_args cli_args)
telltale_add_suite(test_crc32 crc32)
telltale_add_suite(test_logging logging)
