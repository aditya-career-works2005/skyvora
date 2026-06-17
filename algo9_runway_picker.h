#pragma once
// algo9_runway_picker.h -- Algorithm 9: Auto Runway Picker
// Kisi bhi flight ke liye sabse best runway automatically choose karta hai
//
// Normal flights ke liye:
//   1. Free runway (available hai, maintenance nahi)
//   2. Sabse chhoti queue wali runway
//
// Emergency flights ke liye:
//   1. Free runway -- seedha land karo
//   2. Normal flight wali runway -- preempt ho sakti hai
//   3. Sabse chhoti queue wali runway

#include "structs.h"
#include <vector>
#include <climits>

inline Runway* pickBest(std::vector<Runway>& runways, const Flight& f) {
    // Pass 1: free runway dhundho -- sabse best option yahi hai
    for (auto& r : runways)
        if (!r.maintenance && r.available) return &r;

    // Pass 2: emergency ke liye preemptable runway dhundho (normal flight wali)
    // Actual preemption caller karega tryPreempt se -- hum sirf runway return kar rahe hain
    if (f.type == "emergency") {
        for (auto& r : runways) {
            if (r.maintenance || r.current_flight_id == -1) continue;
            return &r;
        }
    }

    // Pass 3: koi free ya preemptable nahi mili -- sabse chhoti queue wali lo
    Runway* best     = nullptr;
    int     shortest = INT_MAX;
    for (auto& r : runways) {
        if (r.maintenance) continue;
        int ql = (int)r.wait_queue.size();
        if (ql < shortest) { shortest = ql; best = &r; }
    }
    return best;
}
