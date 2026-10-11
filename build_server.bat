@echo off
rem Builds the local web server: relief_server.exe. Run from the repository root.
rem Needs server\httplib.h and server\json.hpp (see README.md, "Web server").
if not exist server\httplib.h goto missing
if not exist server\json.hpp goto missing
g++ -std=c++17 -Wall -Wextra -g -I engine -I server ^
  engine\Types.cpp engine\Queue.cpp engine\RequestList.cpp engine\DeliveryLog.cpp engine\Sorting.cpp engine\AVLTree.cpp ^
  engine\Graph.cpp engine\Dijkstra.cpp engine\Engine.cpp engine\Scenario.cpp ^
  server\Api.cpp server\main.cpp -o relief_server.exe -lws2_32
if %ERRORLEVEL% NEQ 0 (
    echo Compilation failed.
    exit /b %ERRORLEVEL%
)
echo Server built successfully. Run relief_server.exe from this folder, then open http://localhost:8080/
exit /b 0

:missing
echo server\httplib.h or server\json.hpp is missing. See README.md, "Web server", for the two download commands.
exit /b 1
