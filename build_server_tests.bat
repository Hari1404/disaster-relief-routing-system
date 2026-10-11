@echo off
rem Builds and runs the tests of the server's API layer (no sockets, no httplib). Run from the repository root.
rem Needs server\json.hpp (see README.md, "Web server").
if not exist server\json.hpp goto missing
g++ -std=c++17 -Wall -Wextra -g -I engine -I tests -I server ^
  engine\Types.cpp engine\Queue.cpp engine\RequestList.cpp engine\DeliveryLog.cpp engine\Sorting.cpp engine\AVLTree.cpp ^
  engine\Graph.cpp engine\Dijkstra.cpp engine\Engine.cpp engine\Scenario.cpp ^
  server\Api.cpp tests\server\test_api.cpp -o run_server_tests.exe
if %ERRORLEVEL% NEQ 0 (
    echo Compilation failed.
    exit /b %ERRORLEVEL%
)
run_server_tests.exe
exit /b %ERRORLEVEL%

:missing
echo server\json.hpp is missing. See README.md, "Web server", for the download command.
exit /b 1
