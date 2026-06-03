CXX      = g++
CXXFLAGS = -Wall -Werror -Wextra -pedantic -std=c++17 -Iinclude
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
TEST_SRC = $(TEST_DIR)/test_telltale.cpp

LIB_OBJS  = $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(LIB_SRCS))
MAIN_OBJ  = $(BUILD_DIR)/main.o
TEST_OBJ  = $(BUILD_DIR)/test_telltale.o

TELLTALE_BIN = telltale
TEST_BIN     = build/test_telltale

.PHONY: all clean test

all: $(TELLTALE_BIN)

$(TELLTALE_BIN): $(LIB_OBJS) $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(LIB_OBJS) $(MAIN_OBJ) $(LDFLAGS)

$(TEST_BIN): $(LIB_OBJS) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(LIB_OBJS) $(TEST_OBJ) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(TEST_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

test: $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -rf $(BUILD_DIR) $(TELLTALE_BIN)
