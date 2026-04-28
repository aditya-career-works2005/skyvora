#pragma once
// algo7_emergency_preemption.h -- Algorithm 7: Emergency Preemption
// Emergency flight aaya toh runway pe baitha normal flight hata do -- ye aaise hota hai preempt
// Hataya hua normal flight pehle free runway dhundega, nahi mila toh queue mein jaayega

#include "structs.h"
#include "algo3_edf_sort.h"
#include <vector>
#include <climits>

inline Flight* findFlight_preempt(std::vector<Flight>& v, int id) {
    for (auto& f : v) if (f.id == id) return &f;
    return nullptr;
}

// true return karta hai agar preemption successful raha
// allRunways isliye chahiye taaki displaced flight ke liye free runway dhund sakein
inline bool tryPreempt(std::vector<Flight>& flights,
                       std::vector<Runway>& allRunways,
                       Runway& rwy,
                       int incoming_id) {
    // agar runway already khali hai toh preemption ki zarurat hi nahi
    if (rwy.current_flight_id == -1) return false;

    Flight* inc = findFlight_preempt(flights, incoming_id);
    Flight* onR = findFlight_preempt(flights, rwy.current_flight_id);
    if (!inc || !onR) return false;

    // sirf emergency, normal ko hata sakta hai -- ye rule hai
    if (inc->type != "emergency" || onR->type != "normal") return false;

    // Step 1: emergency flight ko is runway pe land karo
    rwy.current_flight_id = incoming_id;
    rwy.available         = false;
    inc->status    = "allocated";
    inc->runway_id = rwy.id;

    // Step 2: hataye gaye normal flight ke liye alag free runway dhundho
    Runway* free_rwy = nullptr;
    for (auto& r : allRunways) {
        if (r.id == rwy.id)    continue;  // apni hi runway skip karo
        if (r.maintenance)     continue;  // maintenance wali skip karo
        if (r.available && r.current_flight_id == -1) {
            free_rwy = &r;
            break;
        }
    }

    if (free_rwy) {
        // lucky hai -- free runway mili, seedha land karo
        free_rwy->available         = false;
        free_rwy->current_flight_id = onR->id;
        onR->runway_id = free_rwy->id;
        onR->status    = "allocated";  // landing now, queued nahi
    } else {
        // koi free runway nahi mila -- sabse chhoti queue wali runway mein daalo
        Runway* best     = nullptr;
        int     shortest = INT_MAX;
        for (auto& r : allRunways) {
            if (r.maintenance) continue;
            int ql = (int)r.wait_queue.size();
            if (ql < shortest) { shortest = ql; best = &r; }
        }
        if (best) {
            onR->status    = "queued";
            onR->runway_id = best->id;
            best->wait_queue.push_back(onR->id);
            sortEDF(flights, *best);  // queue ko phir se sort karo EDF mein
        }
    }

    return true;
}
