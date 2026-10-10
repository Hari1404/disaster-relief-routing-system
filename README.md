# ReliefNet - Phase 1

This is the request-handling half of the ReliefNet disaster-relief simulation engine.

## Build and Run Tests (Windows)

To build and run tests, simply execute the batch file:

```cmd
build_and_test.bat
```

This will run the following compiler command:

```cmd
g++ -std=c++17 -Wall -Wextra -g -I engine -I tests ^
  engine\Types.cpp engine\Queue.cpp engine\RequestList.cpp engine\DeliveryLog.cpp engine\Sorting.cpp ^
  tests\test_types.cpp tests\test_queue.cpp tests\test_request_list.cpp tests\test_sorting.cpp ^
  tests\test_delivery_log.cpp tests\run_tests.cpp -o run_tests.exe
```
