CXX=c++
CXXFLAGS=-std=c++11 -Iinclude

OBJECTS=$(foreach f, $(basename $(wildcard src/*.cc)), $(f).o)
TEST_OBJECTS=$(foreach f, $(basename $(wildcard test/*.cc)), $(f).o)

BIN_DIR=bin

.PHONY: clean gmock_clean

all: prepare test

prepare:
	mkdir -p $(BIN_DIR)

test: gmock/libgmock.a $(TEST_OBJECTS) $(OBJECTS)
	$(CXX) -o $(BIN_DIR)/test gmock/libgmock.a $(TEST_OBJECTS) $(OBJECTS) -lpthread

gmock/libgmock.a:
	$(CXX) -Igtest -c -o gtest/src/gtest-all.o gtest/src/gtest-all.cc
	$(CXX) -Igmock -c -o gmock/src/gmock-all.o gmock/src/gmock-all.cc
	$(CXX) -Igmock -c -o gmock/src/gmock_main.o gmock/src/gmock_main.cc
	ar -rv gmock/libgmock.a gmock/src/gmock-all.o gmock/src/gmock_main.o gtest/src/gtest-all.o

gmock_clean:
	rm -f gtest/src/*.o
	rm -f gmock/src/*.o
	rm -f gmock/*.a

clean: gmock_clean
	rm -f $(OBJECTS)
	rm -f $(TEST_OBJECTS)
	rm -f $(GMOCK_OBJECTS)
	rm -rf $(BIN_DIR)
