CXX=c++
CXXFLAGS=-std=c++11 -Iinclude

OBJECTS=$(foreach f, $(basename $(wildcard src/*.cc)), $(f).o)
TEST_OBJECTS=$(foreach f, $(basename $(wildcard test/*.cc)), $(f).o)
BIN_DIR=bin

all: prepare main test

run_test: test
	@$(BIN_DIR)/test

prepare:
	mkdir -p $(BIN_DIR)

main: $(OBJECTS)
	$(CXX) -o $(BIN_DIR)/main $(OBJECTS)

test: $(TEST_OBJECTS)
	$(CXX) -o $(BIN_DIR)/test $(TEST_OBJECTS)

.PHONY: clean
clean:
	rm -f $(OBJECTS)
	rm -f $(TEST_OBJECTS)
	rm -rf $(BIN_DIR)
