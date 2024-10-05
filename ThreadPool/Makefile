CXX = g++
CXXFLAGS = -fsanitize=thread -std=c++20 -Wall -O3 -I.
LDFLAGS = -pthread
GTEST_LDFLAGS = -fsanitize=thread -lgtest -lgtest_main
OBJ_DIR = ./obj
SRC_DIR = .

# Source files that are not tests
SOURCES = ThreadPool.cpp
OBJECTS = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SOURCES))

# Test files
TEST_SRC = tests.cpp
TEST_OBJ = $(OBJ_DIR)/tests.o
TEST_EXEC = tests

# Ensure the object directory exists
$(shell mkdir -p $(OBJ_DIR))

all: $(OBJECTS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

tests: $(TEST_EXEC)

$(TEST_EXEC): $(TEST_OBJ) $(OBJECTS)
	$(CXX) $(LDFLAGS) $^ $(GTEST_LDFLAGS) -o $@
	./$(TEST_EXEC)

$(TEST_OBJ): $(TEST_SRC)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)/* $(TEST_EXEC)

.PHONY: all tests clean
