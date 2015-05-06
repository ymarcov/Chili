CXX=c++
CXXFLAGS=-std=c++1y -Iinclude -g

OBJECTS=$(foreach f, $(basename $(wildcard src/*.cc)), $(f).o)
TEST_OBJECTS=$(foreach f, $(basename $(wildcard test/*.cc)), $(f).o)

BIN_DIR=bin

.PHONY: clean ext/gmock_clean

all: prepare test run_test

prepare:
	mkdir -p $(BIN_DIR)

run_test:
	@bin/test

test: ext/gmock/libgmock.a $(TEST_OBJECTS) $(OBJECTS)
	$(CXX) -o $(BIN_DIR)/test ext/gmock/libgmock.a $(TEST_OBJECTS) $(OBJECTS) -lpthread

ext/gmock/libgmock.a:
	$(CXX) -Iext/gtest -c -o ext/gtest/src/gtest-all.o ext/gtest/src/gtest-all.cc
	$(CXX) -Iext/gmock -c -o ext/gmock/src/gmock-all.o ext/gmock/src/gmock-all.cc
	$(CXX) -Iext/gmock -c -o ext/gmock/src/gmock_main.o ext/gmock/src/gmock_main.cc
	ar -rv ext/gmock/libgmock.a ext/gmock/src/gmock-all.o ext/gmock/src/gmock_main.o ext/gtest/src/gtest-all.o

libgmock_clean:
	rm -f ext/gtest/src/*.o
	rm -f ext/gmock/src/*.o
	rm -f ext/gmock/*.a

clean: libgmock_clean
	rm -f $(OBJECTS)
	rm -f $(TEST_OBJECTS)
	rm -f $(ext/gmock_OBJECTS)
	rm -rf $(BIN_DIR)
