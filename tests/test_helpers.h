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
void runGraphTests();
void runDijkstraTests();
void runEngineTests();
void runScenarioTests();

#include <string>
#include <fstream>
#include <sstream>

inline std::string readTextFile(const char* path) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return "";
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::string buffer;
    if (size > 0) {
        buffer.resize(size);
        std::fread(&buffer[0], 1, size, f);
    }
    std::fclose(f);
    return buffer;
}
