@echo off
g++ -std=c++17 -Wall -Wextra -g -I engine -I tests ^
  engine\Types.cpp engine\Queue.cpp engine\RequestList.cpp engine\DeliveryLog.cpp engine\Sorting.cpp engine\AVLTree.cpp engine\Graph.cpp engine\Dijkstra.cpp engine\Engine.cpp engine\Scenario.cpp ^
  tests\test_types.cpp tests\test_queue.cpp tests\test_request_list.cpp tests\test_sorting.cpp ^
  tests\test_delivery_log.cpp tests\test_avl.cpp tests\test_graph.cpp tests\test_dijkstra.cpp tests\test_engine.cpp tests\test_scenarios.cpp tests\run_tests.cpp -o run_tests.exe
if %ERRORLEVEL% NEQ 0 (
    echo Compilation failed.
    exit /b %ERRORLEVEL%
)
run_tests.exe
exit /b %ERRORLEVEL%
