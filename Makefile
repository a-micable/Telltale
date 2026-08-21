CXX      = g++
CXXFLAGS = -Wall -Werror -Wextra -pedantic -std=c++17 -Iinclude -Itests
LDFLAGS  =

SRC_DIR   = src
TEST_DIR  = tests
BUILD_DIR = build

LIB_SRCS = \
	$(SRC_DIR)/crc32.cpp \
	$(SRC_DIR)/binary_io.cpp \
	$(SRC_DIR)/handler_registry.cpp \
	$(SRC_DIR)/schema_update.cpp \
	$(SRC_DIR)/dispatcher.cpp \
	$(SRC_DIR)/builtin_handlers.cpp \
	$(SRC_DIR)/filter_engine.cpp \
	$(SRC_DIR)/diff_engine.cpp \
	$(SRC_DIR)/compaction_engine.cpp \
	$(SRC_DIR)/text_format.cpp \
	$(SRC_DIR)/cli.cpp

MAIN_SRC = $(SRC_DIR)/main.cpp

# Hand-rolled suite only (ignore unrelated gtest stubs under tests/).
TEST_SRCS = \
	$(TEST_DIR)/test_common.cpp \
	$(TEST_DIR)/test_binary_io.cpp \
	$(TEST_DIR)/test_dispatcher.cpp \
	$(TEST_DIR)/test_filter_engine.cpp \
	$(TEST_DIR)/test_text_format.cpp \
	$(TEST_DIR)/test_telltale.cpp

LIB_OBJS  = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(LIB_SRCS))
MAIN_OBJ  = $(BUILD_DIR)/main.o
TEST_OBJS = $(patsubst $(TEST_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(TEST_SRCS))

TELLTALE_BIN = telltale
TEST_BIN     = build/test_telltale

.PHONY: all clean test corpus

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

test: $(TEST_BIN)
	./$(TEST_BIN)

corpus: $(CORPUS_GEN)
	./$(CORPUS_GEN)

$(CORPUS_GEN): fuzz/generate_corpus.cpp $(CORPUS_GEN_OBJS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ fuzz/generate_corpus.cpp $(CORPUS_GEN_OBJS) $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) $(TELLTALE_BIN)
