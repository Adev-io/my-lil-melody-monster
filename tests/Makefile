CXX ?= g++
CXXFLAGS ?= -std=c++11 -O1 -Wall -Wextra -I../core
CORE_SRC := $(wildcard ../core/*.cpp)
TEST_SRC := $(wildcard test_*.cpp) main.cpp
BIN := ../build/tests.exe

all: run

$(BIN): $(TEST_SRC) $(CORE_SRC) $(wildcard ../core/*.hpp) t.hpp
	@mkdir -p ../build
	$(CXX) $(CXXFLAGS) $(TEST_SRC) $(CORE_SRC) -o $(BIN)

run: $(BIN)
	cd .. && ./build/tests.exe

.PHONY: all run
golden: $(BIN)
	cd .. && ./build/tests.exe --write-golden
.PHONY: golden
