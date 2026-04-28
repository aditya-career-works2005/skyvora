#pragma once
// algo5_runway_exclusion.h -- Algorithm 5: Runway Exclusion
// Ek runway pe ek waqt mein sirf ek flight allowed hai -- ye aaise hota hai exclusion
// Agar runway free hai toh seedha land karo, warna queue mein daalo

#include "structs.h"
#include <vector>

inline Flight* findFlight_excl(std::vector<Flight>& v, int id) {
    for (auto& f : v) if (f.id == id) return &f;
    return nullptr;
}

// true return karta hai agar flight allocated ho gayi (land ho rahi hai abhi)
// false return karta hai agar runway busy tha aur flight queue mein gayi
inline bool tryLand(std::vector<Flight>& flights, Runway& rwy, int fid) {
    Flight* f = findFlight_excl(flights, fid);
    if (!f) return false;

    if (rwy.available) {
        // runway khali hai -- sidha land karo
        rwy.available         = false;
        rwy.current_flight_id = fid;
        f->status    = "allocated";
        f->runway_id = rwy.id;
        return true;
    } else {
        // runway busy hai -- queue mein daalo bhai
        rwy.wait_queue.push_back(fid);
        f->status    = "queued";
        f->runway_id = rwy.id;
        return false;
    }
}
