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
    runSuite("Types", runTypesTests);
    runSuite("Queue", runQueueTests);
    runSuite("RequestList", runRequestListTests);
    runSuite("Sorting", runSortingTests);
    runSuite("DeliveryLog", runDeliveryLogTests);
    runSuite("AVL", runAVLTests);
    
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    
    return g_failures > 0 ? 1 : 0;
}
