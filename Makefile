CMAKE=cmake
CXX=c++
CXXFLAGS=-std=c++1y -Iinclude -g

OBJECTS=$(foreach f, $(basename $(wildcard src/*.cc)), $(f).o)
TEST_OBJECTS=$(foreach f, $(basename $(wildcard test/*.cc)), $(f).o)
SANDBOX_OBJECTS=$(foreach f, $(basename $(wildcard sandbox/*.cc)), $(f).o)

BIN_DIR=bin


.PHONY: clean ext/gmock_clean

all: prepare test run_test

prepare:
	mkdir -p $(BIN_DIR)

run_test:
	@bin/test

test: ext/gmock/libgmock.a $(TEST_OBJECTS) $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(BIN_DIR)/test $(TEST_OBJECTS) $(OBJECTS) ext/gmock/libgmock.a -lpthread

sandbox: $(SANDBOX_OBJECTS) $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(BIN_DIR)/sandbox $(SANDBOX_OBJECTS) $(OBJECTS) -lpthread
	@bin/sandbox

sb: sandbox

ext/gmock/libgmock.a:
	$(CXX) -Iext/gtest -c -o ext/gtest/src/gtest-all.o ext/gtest/src/gtest-all.cc
	$(CXX) -Iext/gmock -c -o ext/gmock/src/gmock-all.o ext/gmock/src/gmock-all.cc
	$(CXX) -Iext/gmock -c -o ext/gmock/src/gmock_main.o ext/gmock/src/gmock_main.cc
	ar -rv ext/gmock/libgmock.a ext/gmock/src/gmock-all.o ext/gmock/src/gmock_main.o ext/gtest/src/gtest-all.o

libgmock_clean:
	rm -f ext/gtest/src/*.o
	rm -f ext/gmock/src/*.o
	rm -f ext/gmock/*.a

clean_own:
	rm -f $(OBJECTS)
	rm -f $(TEST_OBJECTS)
	rm -rf $(BIN_DIR)

clean: libgmock_clean clean_own
	rm -f $(ext/gmock_OBJECTS)
