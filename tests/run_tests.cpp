#include "test_helpers.h"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <string>

int g_checks = 0;
int g_failures = 0;

static long long g_allocations = 0;
static long long g_frees = 0;

void* operator new(std::size_t size) {
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    g_allocations++;
    return p;
}

void operator delete(void* p) noexcept {
    if (p) {
        g_frees++;
        std::free(p);
    }
}

void operator delete(void* p, std::size_t) noexcept {
    if (p) {
        g_frees++;
        std::free(p);
    }
}

void* operator new[](std::size_t size) {
    void* p = std::malloc(size);
    if (!p) throw std::bad_alloc();
    g_allocations++;
    return p;
}

void operator delete[](void* p) noexcept {
    if (p) {
        g_frees++;
        std::free(p);
    }
}

void operator delete[](void* p, std::size_t) noexcept {
    if (p) {
        g_frees++;
        std::free(p);
    }
}

static void runSuite(const char* name, void (*func)()) {
    std::printf("--- %s ---\n", name);
    long long allocsBefore = g_allocations;
    long long freesBefore = g_frees;
    
    func();
    
    long long allocsAfter = g_allocations;
    long long freesAfter = g_frees;
    
    long long allocsDuring = allocsAfter - allocsBefore;
    long long freesDuring = freesAfter - freesBefore;
    
    if (allocsDuring != freesDuring) {
        std::printf("  FAIL LEAK in %s: %lld allocations but %lld frees\n", name, allocsDuring, freesDuring);
        g_failures++;
    }
}

int main() {
    // Warm-up: use the file, stream and string-conversion code once before the first suite.
    // The standard library makes small one-time allocations the first time these are used,
    // and the leak counter in runSuite would report them as leaks.
    readTextFile("README.md");
    {
        int a; double b; std::string w;
        std::istringstream ss("123 45.6 hello\nworld");
        ss >> a >> b >> w;
        std::getline(ss, w);
        volatile std::string dummy = std::to_string(123456789);
    }
    
    runSuite("Types", runTypesTests);
    runSuite("Queue", runQueueTests);
    runSuite("RequestList", runRequestListTests);
    runSuite("Sorting", runSortingTests);
    runSuite("DeliveryLog", runDeliveryLogTests);
    runSuite("AVL", runAVLTests);
    runSuite("Graph", runGraphTests);
    runSuite("Dijkstra", runDijkstraTests);
    runSuite("Engine", runEngineTests);
    runSuite("Scenarios", runScenarioTests);
    
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    
    return g_failures > 0 ? 1 : 0;
}
