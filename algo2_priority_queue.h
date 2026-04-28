#pragma once
// algo2_priority_queue.h -- Algorithm 2: Priority Queue
// Emergency flights pehle, phir ETA ke order mein sort hota hai -- ye aaise hota hai priority

#include "structs.h"
#include "algo1_queue.h"
#include <algorithm>
using std::sort;

class PriorityQueue {
    Queue<Flight> pq;
public:
    // naya flight add karo aur phir sort karo taaki order sahi rahe
    void enqueue(const Flight& item) {
        // pehle existing flights ek array mein nikaalo
        Flight tmp[100];
        int cnt = 0;
        for (Flight* f = pq.front(); f && cnt < 99; f = pq.next(f))
            tmp[cnt++] = *f;

        // naya item add karo array mein
        tmp[cnt++] = item;

        // sort karo -- emergency pehle, phir ETA ke hisaab se
        sort(tmp, tmp + cnt, [](const Flight& a, const Flight& b) {
            if (a.type != b.type) return a.type == "emergency";
            return a.eta < b.eta;
        });

        // purani queue saaf karo
        while (!pq.isEmpty()) delete pq.dequeue();

        // sorted order mein wapas daalo
        for (int i = 0; i < cnt; i++) pq.enqueue(tmp[i]);
    }

    Flight* front()           { return pq.front(); }
    Flight* next(Flight* cur) { return pq.next(cur); }
    bool    isEmpty()         { return pq.isEmpty(); }
};
