# ReliefNet build. Run make from the repository root: the tests and the console
# read data/scenario*.txt by relative path.
#
#   make test        build and run the unit and scenario tests
#   make test-san    the same, with AddressSanitizer and UBSan switched on
#   make console     build the interactive console (./relief_console)
#   make server      build the local web server (./relief_server). It needs the two single-header
#                    libraries server/httplib.h and server/json.hpp, see README.md ("Web server")
#   make serve       build and start the server on http://localhost:8080
#   make test-server        build and run the tests of the server's API layer
#   make test-server-san    the same, with AddressSanitizer and UBSan
#   make server-san         build ./relief_server_san (the whole server with the sanitizers on)
#   make test-console       build the console and run its input-handling regression test (BUG-7)
#   make test-all    run test, test-server and test-console
#   make clean       remove build output

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -g

ENGINE_SRC  := $(wildcard engine/*.cpp)
TEST_SRC    := $(wildcard tests/*.cpp)
CONSOLE_SRC := console/console_main.cpp
HEADERS     := $(wildcard engine/*.h tests/*.h)

# ---- server (Phase 3) ----
# httplib needs the Windows socket library on Windows and pthreads elsewhere.
ifeq ($(OS),Windows_NT)
SERVER_LIBS := -lws2_32
else
SERVER_LIBS := -pthread
endif
SERVER_HEADERS := server/Api.h server/httplib.h server/json.hpp
SERVER_SRC     := server/Api.cpp server/main.cpp
APITEST_SRC    := tests/server/test_api.cpp

.PHONY: all test test-san console server serve server-san test-server test-server-san test-console test-all clean

all: test console

run_tests: $(ENGINE_SRC) $(TEST_SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -I engine -I tests $(ENGINE_SRC) $(TEST_SRC) -o $@

run_tests_san: $(ENGINE_SRC) $(TEST_SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -fsanitize=address,undefined -I engine -I tests $(ENGINE_SRC) $(TEST_SRC) -o $@

relief_console: $(ENGINE_SRC) $(CONSOLE_SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -I engine $(ENGINE_SRC) $(CONSOLE_SRC) -o $@

# The two vendored single headers are not downloaded by make. If one is missing, say how to get it.
server/httplib.h:
	@echo "Missing server/httplib.h. Download it (see README.md, 'Web server'):"
	@echo "  curl -L -o server/httplib.h https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.18.7/httplib.h"
	@exit 1

server/json.hpp:
	@echo "Missing server/json.hpp. Download it (see README.md, 'Web server'):"
	@echo "  curl -L -o server/json.hpp https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp"
	@exit 1

relief_server: $(ENGINE_SRC) $(SERVER_SRC) $(HEADERS) $(SERVER_HEADERS)
	$(CXX) $(CXXFLAGS) -I engine -I server $(ENGINE_SRC) $(SERVER_SRC) -o $@ $(SERVER_LIBS)

relief_server_san: $(ENGINE_SRC) $(SERVER_SRC) $(HEADERS) $(SERVER_HEADERS)
	$(CXX) $(CXXFLAGS) -fsanitize=address,undefined -I engine -I server $(ENGINE_SRC) $(SERVER_SRC) -o $@ $(SERVER_LIBS)

run_server_tests: $(ENGINE_SRC) server/Api.cpp $(APITEST_SRC) $(HEADERS) server/Api.h server/json.hpp
	$(CXX) $(CXXFLAGS) -I engine -I tests -I server $(ENGINE_SRC) server/Api.cpp $(APITEST_SRC) -o $@

run_server_tests_san: $(ENGINE_SRC) server/Api.cpp $(APITEST_SRC) $(HEADERS) server/Api.h server/json.hpp
	$(CXX) $(CXXFLAGS) -fsanitize=address,undefined -I engine -I tests -I server $(ENGINE_SRC) server/Api.cpp $(APITEST_SRC) -o $@

test: run_tests
	./run_tests

test-san: run_tests_san
	./run_tests_san

console: relief_console

server: relief_server

server-san: relief_server_san

serve: relief_server
	./relief_server

test-server: run_server_tests
	./run_server_tests

test-server-san: run_server_tests_san
	./run_server_tests_san

test-console: relief_console
	bash tests/console/console_input_test.sh ./relief_console

test-all: test test-server test-console

clean:
	rm -f run_tests run_tests.exe run_tests_san relief_console relief_console.exe
	rm -f relief_server relief_server.exe relief_server_san run_server_tests run_server_tests.exe run_server_tests_san
