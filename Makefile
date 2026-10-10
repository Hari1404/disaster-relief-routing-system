# ReliefNet build. Run make from the repository root: the tests and the console
# read data/scenario*.txt by relative path.
#
#   make test        build and run the unit and scenario tests
#   make test-san    the same, with AddressSanitizer and UBSan switched on
#   make console     build the interactive console (./relief_console)
#   make clean       remove build output

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -g

ENGINE_SRC  := $(wildcard engine/*.cpp)
TEST_SRC    := $(wildcard tests/*.cpp)
CONSOLE_SRC := console/console_main.cpp
HEADERS     := $(wildcard engine/*.h tests/*.h)

.PHONY: all test test-san console clean

all: test console

run_tests: $(ENGINE_SRC) $(TEST_SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -I engine -I tests $(ENGINE_SRC) $(TEST_SRC) -o $@

run_tests_san: $(ENGINE_SRC) $(TEST_SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -fsanitize=address,undefined -I engine -I tests $(ENGINE_SRC) $(TEST_SRC) -o $@

relief_console: $(ENGINE_SRC) $(CONSOLE_SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -I engine $(ENGINE_SRC) $(CONSOLE_SRC) -o $@

test: run_tests
	./run_tests

test-san: run_tests_san
	./run_tests_san

console: relief_console

clean:
	rm -f run_tests run_tests.exe run_tests_san relief_console relief_console.exe
