#include "test_helpers.h"
#include "Queue.h"

void runQueueTests() {
    // 1. A new queue is empty, size 0; dequeue and peek return false.
    Queue q;
    CHECK(q.isEmpty());
    CHECK(q.size() == 0);
    Request out;
    CHECK(!q.dequeue(out));
    CHECK(!q.peek(out));

    // 2. Enqueue three requests
    Request r1 = makeRequest(1, 1, ResourceType::Food, 10, 2, 5, 1);
    Request r2 = makeRequest(2, 2, ResourceType::Water, 20, 3, 10, 2);
    Request r3 = makeRequest(3, 3, ResourceType::Medicine, 30, 4, 15, 3);
    q.enqueue(r1);
    q.enqueue(r2);
    q.enqueue(r3);
    CHECK(q.size() == 3);
    CHECK(!q.isEmpty());
    
    CHECK(q.dequeue(out) && out.id == 1);
    CHECK(q.dequeue(out) && out.id == 2);
    CHECK(q.dequeue(out) && out.id == 3);
    CHECK(q.isEmpty());
    CHECK(q.size() == 0);

    // 3. peek returns the front and does not remove it
    q.enqueue(r1);
    CHECK(q.peek(out) && out.id == 1);
    CHECK(q.size() == 1);
    q.dequeue(out);

    // 4. Interleaved: enqueue A, B; dequeue gives A; enqueue C; dequeue gives B, then C
    q.enqueue(r1);
    q.enqueue(r2);
    CHECK(q.dequeue(out) && out.id == 1);
    q.enqueue(r3);
    CHECK(q.dequeue(out) && out.id == 2);
    CHECK(q.dequeue(out) && out.id == 3);
    CHECK(q.isEmpty());

    // 5. removeByCamp
    // remove from the front
    q.enqueue(makeRequest(1, 10, ResourceType::Food, 1, 1, 1, 1));
    q.enqueue(makeRequest(2, 20, ResourceType::Food, 1, 1, 1, 2));
    q.enqueue(makeRequest(3, 30, ResourceType::Food, 1, 1, 1, 3));
    CHECK(q.removeByCamp(10) == 1);
    CHECK(q.size() == 2);
    auto vec = q.toVector();
    CHECK(vec.size() == 2 && vec[0].id == 2 && vec[1].id == 3);
    q.clear();

    // remove from the middle
    q.enqueue(makeRequest(1, 10, ResourceType::Food, 1, 1, 1, 1));
    q.enqueue(makeRequest(2, 20, ResourceType::Food, 1, 1, 1, 2));
    q.enqueue(makeRequest(3, 30, ResourceType::Food, 1, 1, 1, 3));
    CHECK(q.removeByCamp(20) == 1);
    CHECK(q.size() == 2);
    vec = q.toVector();
    CHECK(vec.size() == 2 && vec[0].id == 1 && vec[1].id == 3);
    q.clear();

    // remove from the back
    q.enqueue(makeRequest(1, 10, ResourceType::Food, 1, 1, 1, 1));
    q.enqueue(makeRequest(2, 20, ResourceType::Food, 1, 1, 1, 2));
    q.enqueue(makeRequest(3, 30, ResourceType::Food, 1, 1, 1, 3));
    CHECK(q.removeByCamp(30) == 1);
    CHECK(q.size() == 2);
    vec = q.toVector();
    CHECK(vec.size() == 2 && vec[0].id == 1 && vec[1].id == 2);
    q.clear();

    // several in a row
    q.enqueue(makeRequest(1, 10, ResourceType::Food, 1, 1, 1, 1));
    q.enqueue(makeRequest(2, 20, ResourceType::Food, 1, 1, 1, 2));
    q.enqueue(makeRequest(3, 20, ResourceType::Food, 1, 1, 1, 3));
    q.enqueue(makeRequest(4, 30, ResourceType::Food, 1, 1, 1, 4));
    CHECK(q.removeByCamp(20) == 2);
    CHECK(q.size() == 2);
    vec = q.toVector();
    CHECK(vec.size() == 2 && vec[0].id == 1 && vec[1].id == 4);
    q.clear();

    // all of them
    q.enqueue(makeRequest(1, 10, ResourceType::Food, 1, 1, 1, 1));
    q.enqueue(makeRequest(2, 10, ResourceType::Food, 1, 1, 1, 2));
    CHECK(q.removeByCamp(10) == 2);
    CHECK(q.size() == 0);
    vec = q.toVector();
    CHECK(vec.empty());

    // a camp with no requests
    q.enqueue(makeRequest(1, 10, ResourceType::Food, 1, 1, 1, 1));
    CHECK(q.removeByCamp(99) == 0);
    CHECK(q.size() == 1);
    
    // 6. After removeByCamp removes the last node, enqueue still works and tail is correct
    q.clear();
    q.enqueue(makeRequest(1, 10, ResourceType::Food, 1, 1, 1, 1));
    q.removeByCamp(10);
    q.enqueue(makeRequest(2, 20, ResourceType::Food, 1, 1, 1, 2));
    CHECK(q.size() == 1);
    CHECK(q.dequeue(out) && out.id == 2);

    // 7. toVector returns front-to-back order
    q.enqueue(r1);
    q.enqueue(r2);
    vec = q.toVector();
    CHECK(vec.size() == 2 && vec[0].id == 1 && vec[1].id == 2);

    // 8. clear empties the queue and it can be reused
    q.clear();
    CHECK(q.isEmpty() && q.size() == 0);
    q.enqueue(r1);
    CHECK(q.size() == 1);
    q.clear();

    // 9. Stress: enqueue 1000 requests and dequeue all 1000 in the correct order
    for (int i = 1; i <= 1000; ++i) {
        q.enqueue(makeRequest(i, i, ResourceType::Food, 1, 1, 1, i));
    }
    CHECK(q.size() == 1000);
    for (int i = 1; i <= 1000; ++i) {
        bool ok = q.dequeue(out);
        CHECK(ok && out.id == i);
    }
    CHECK(q.isEmpty());
}
