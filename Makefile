# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++20 -I include
LDFLAGS = -lrtlsdr
TEST_LDFLAGS = $(LDFLAGS) -lCatch2Main -lCatch2

# Directories
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = bin

# Target executable
TARGET = $(BIN_DIR)/ads-b-decoder
TEST_TARGET = $(BIN_DIR)/tests

# Source and object files
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# Default target
all: $(TARGET)

# Link object files to create executable
$(TARGET): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

# Compile source files to object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

TEST_SRC = tests/tests.cpp
TEST_OBJ = $(OBJ_DIR)/tests.o

$(OBJ_DIR)/tests.o: $(TEST_SRC)
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST_TARGET): $(filter-out $(OBJ_DIR)/main.o, $(OBJECTS)) $(TEST_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CXX) $^ -o $@ $(TEST_LDFLAGS)

bin/tests: $(filter-out $(OBJ_DIR)/main.o, $(OBJECTS)) $(TEST_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CXX) $^ -o $@ $(TEST_LDFLAGS)

# Run the application
run: $(TARGET)
	$(TARGET)

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

test: bin/tests
	./bin/tests
# Phony targets
.PHONY: all clean test