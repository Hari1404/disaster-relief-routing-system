#pragma once
#include <cstdio>
#include "Types.h"

extern int g_checks;
extern int g_failures;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failures; \
    std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

inline Request makeRequest(int id, int camp, ResourceType resource, int qty, int urgency, int families, long seq) {
    Request r;
    r.id = id;
    r.campId = camp;
    r.resource = resource;
    r.quantity = qty;
    r.urgency = urgency;
    r.families = families;
    r.arrivalSeq = seq;
    return r;
}

void runTypesTests();
void runQueueTests();
void runRequestListTests();
void runSortingTests();
void runDeliveryLogTests();
void runAVLTests();
