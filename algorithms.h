#pragma once
// algorithms.h -- Flight Runway Management System
// Umbrella header -- yahan se saare 10 algorithm files include hote hain.
// Har algorithm apni dedicated file mein hai taaki padhna aasaan ho.

#include "algo1_queue.h"                // Algorithm 1  : Linked-List Queue
#include "algo2_priority_queue.h"       // Algorithm 2  : Priority Queue (emergency-first, ETA)
#include "algo3_edf_sort.h"             // Algorithm 3  : EDF Sort
#include "algo4_activity_selection.h"   // Algorithm 4  : Activity Selection (Greedy)
#include "algo5_runway_exclusion.h"     // Algorithm 5  : Runway Exclusion (tryLand)
#include "algo6_time_conflict.h"        // Algorithm 6  : Time-Conflict Detection
#include "algo7_emergency_preemption.h" // Algorithm 7  : Emergency Preemption (tryPreempt)
#include "algo8_conflict_resolver.h"    // Algorithm 8  : Queue Conflict Resolver
#include "algo9_runway_picker.h"        // Algorithm 9  : Auto Runway Picker (pickBest)
#include "algo10_history_sort.h"        // Algorithm 10 : History Sort by ID

// Lookup helpers -- handlers.cpp directly inhe use karta hai
#include "structs.h"
#include <vector>

// flight dhundho by id -- nahi mila toh nullptr dega
inline Flight* findFlight(std::vector<Flight>& v, int id) {
    for (auto& f : v) if (f.id == id) return &f;
    return nullptr;
}

// runway dhundho by id -- nahi mila toh nullptr dega
inline Runway* findRunway(std::vector<Runway>& v, int id) {
    for (auto& r : v) if (r.id == id) return &r;
    return nullptr;
}
