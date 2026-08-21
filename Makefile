CXX      = g++
CXXFLAGS = -Wall -Werror -Wextra -pedantic -std=c++17 -Iinclude -Itests
LDFLAGS  = -pthread
COVERAGE_CFLAGS = -fprofile-arcs -ftest-coverage -O0 -g
COVERAGE_LDFLAGS = --coverage

SRC_DIR   = src
TEST_DIR  = tests
BUILD_DIR = build
COVERAGE_DIR = coverage

LIB_SRCS = \
	$(SRC_DIR)/crc32.cpp \
	$(SRC_DIR)/binary_io.cpp \
	$(SRC_DIR)/payload_codec.cpp \
	$(SRC_DIR)/handler_registry.cpp \
	$(SRC_DIR)/schema_update.cpp \
	$(SRC_DIR)/dispatcher.cpp \
	$(SRC_DIR)/builtin_handlers.cpp \
	$(SRC_DIR)/filter_engine.cpp \
	$(SRC_DIR)/payload_fields.cpp \
	$(SRC_DIR)/diff_engine.cpp \
	$(SRC_DIR)/compaction_engine.cpp \
	$(SRC_DIR)/text_export.cpp \
	$(SRC_DIR)/text_import.cpp \
	$(SRC_DIR)/network.cpp \
	$(SRC_DIR)/storage_backends.cpp \
	$(SRC_DIR)/health.cpp \
	$(SRC_DIR)/cli.cpp \
	$(SRC_DIR)/cli_usage.cpp \
	$(SRC_DIR)/cli_sample.cpp

MAIN_SRC = $(SRC_DIR)/main.cpp

# Runnable suite: tests/test_*.cpp plus GoogleTest-compatible macros via tests/gtest/gtest.h
TEST_SRCS = \
	$(TEST_DIR)/test_common.cpp \
	$(TEST_DIR)/test_crc32.cpp \
	$(TEST_DIR)/test_writer.cpp \
	$(TEST_DIR)/test_roundtrip_counter.cpp \
	$(TEST_DIR)/test_roundtrip_keyvalue.cpp \
	$(TEST_DIR)/test_roundtrip_print.cpp \
	$(TEST_DIR)/test_endian.cpp \
	$(TEST_DIR)/test_event_roundtrip.cpp \
	$(TEST_DIR)/test_schema.cpp \
	$(TEST_DIR)/test_registry.cpp \
	$(TEST_DIR)/test_state_crc.cpp \
	$(TEST_DIR)/test_multi_event.cpp \
	$(TEST_DIR)/test_filter_engine.cpp \
	$(TEST_DIR)/test_diff_engine.cpp \
	$(TEST_DIR)/test_compaction_engine.cpp \
	$(TEST_DIR)/test_text_format.cpp \
	$(TEST_DIR)/test_cli_validation.cpp \
	$(TEST_DIR)/test_cli_args.cpp \
	$(TEST_DIR)/test_logging.cpp \
	$(TEST_DIR)/test_managers.cpp \
	$(TEST_DIR)/test_fresh_clone.cpp \
	$(TEST_DIR)/test_filler_cleanup.cpp \
	$(TEST_DIR)/test_health_validation.cpp \
	$(TEST_DIR)/test_telltale.cpp

LIB_OBJS  = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(LIB_SRCS))
MAIN_OBJ  = $(BUILD_DIR)/main.o
TEST_OBJS = $(patsubst $(TEST_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(TEST_SRCS))

TELLTALE_BIN = telltale
TEST_BIN     = build/test_telltale

.PHONY: all clean test check corpus coverage

all: $(TELLTALE_BIN)

CORPUS_GEN = build/generate_corpus
CORPUS_GEN_OBJS = $(BUILD_DIR)/crc32.o $(BUILD_DIR)/binary_io.o $(BUILD_DIR)/schema_update.o

$(TELLTALE_BIN): $(LIB_OBJS) $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(LIB_OBJS) $(MAIN_OBJ) $(LDFLAGS)

$(TEST_BIN): $(LIB_OBJS) $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(LIB_OBJS) $(TEST_OBJS) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(TEST_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Canonical test command (also used by ./scripts/run_tests.sh and CI).
test: $(TEST_BIN)
	./$(TEST_BIN)

check: test

# Rebuild the suite with gcov flags, run it, and emit an HTML + summary report.
# Fails if line coverage of linked library sources drops below 70%.
coverage:
	$(MAKE) clean
	$(MAKE) test CXXFLAGS="$(CXXFLAGS) $(COVERAGE_CFLAGS)" LDFLAGS="$(LDFLAGS) $(COVERAGE_LDFLAGS)"
	mkdir -p $(COVERAGE_DIR)
	@if command -v gcovr >/dev/null 2>&1; then \
	  gcovr --root . \
	    --filter 'src/crc32\.cpp' \
	    --filter 'src/binary_io\.cpp' \
	    --filter 'src/payload_codec\.cpp' \
	    --filter 'src/dispatcher\.cpp' \
	    --filter 'src/handler_registry\.cpp' \
	    --filter 'src/schema_update\.cpp' \
	    --filter 'src/builtin_handlers\.cpp' \
	    --filter 'src/cli\.cpp' \
	    --html-details $(COVERAGE_DIR)/index.html \
	    --txt $(COVERAGE_DIR)/summary.txt \
	    --fail-under-line 70 ; \
	elif command -v lcov >/dev/null 2>&1; then \
	  lcov --capture --directory $(BUILD_DIR) --output-file $(COVERAGE_DIR)/coverage.info \
	    --rc lcov_branch_coverage=0 --ignore-errors mismatch,gcov,unused ; \
	  lcov --remove $(COVERAGE_DIR)/coverage.info '/usr/*' '*/tests/*' \
	    --output-file $(COVERAGE_DIR)/coverage.filtered.info \
	    --rc lcov_branch_coverage=0 --ignore-errors unused ; \
	  genhtml $(COVERAGE_DIR)/coverage.filtered.info --output-directory $(COVERAGE_DIR) \
	    --rc lcov_branch_coverage=0 >/dev/null ; \
	  python3 scripts/check_coverage.py $(COVERAGE_DIR)/coverage.filtered.info 70 ; \
	else \
	  echo "Install gcovr or lcov to generate coverage reports"; exit 1 ; \
	fi

corpus: $(CORPUS_GEN)
	./$(CORPUS_GEN)

$(CORPUS_GEN): fuzz/generate_corpus.cpp $(CORPUS_GEN_OBJS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ fuzz/generate_corpus.cpp $(CORPUS_GEN_OBJS) $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) $(TELLTALE_BIN) $(COVERAGE_DIR)
	find . -name '*.gcda' -o -name '*.gcno' -o -name '*.gcov' | xargs -r rm -f
