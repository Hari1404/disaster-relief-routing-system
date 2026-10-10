@echo off
g++ -std=c++17 -Wall -Wextra -g -I engine ^
  engine\Types.cpp engine\Queue.cpp engine\RequestList.cpp engine\DeliveryLog.cpp engine\Sorting.cpp engine\AVLTree.cpp ^
  engine\Graph.cpp engine\Dijkstra.cpp engine\Engine.cpp engine\Scenario.cpp ^
  console\console_main.cpp -o relief_console.exe
if %ERRORLEVEL% NEQ 0 (
    echo Compilation failed.
    exit /b %ERRORLEVEL%
)
echo Console built successfully. Run relief_console.exe
