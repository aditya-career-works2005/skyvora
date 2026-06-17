#pragma once
// algo8_conflict_resolver.h -- Algorithm 8: Queue Conflict Resolver
// Jab naya flight queue mein aata hai toh existing flights se ETA clash check karo
// Agar normal flight emergency se clash kar rahi hai toh normal ko delay karo
// Ye aaise hota hai conflict resolution -- emergency ko priority milti hai

#include "structs.h"
#include "algo3_edf_sort.h"
#include "algo6_time_conflict.h"
#include <vector>

inline Flight* findFlight_conf(std::vector<Flight>& v, int id) {
    for (auto& f : v) if (f.id == id) return &f;
    return nullptr;
}

// true return karta hai agar koi conflict mila aur resolve kiya
inline bool resolveConflict(std::vector<Flight>& flights, Runway& rwy, int new_id) {
    Flight* nf = findFlight_conf(flights, new_id);
    if (!nf) return false;

    // queue mein saare existing flights se compare karo
    for (int qid : rwy.wait_queue) {
        if (qid == new_id) continue;  // apne aap se compare mat karo
        Flight* qf = findFlight_conf(flights, qid);
        if (!qf) continue;

        // agar ETA close hai aur types alag hain toh conflict hai
        if (timeConflict(nf->eta, qf->eta) && nf->type != qf->type) {
            // normal flight ko DELAY_BUMP minutes aage dhakka maaro
            Flight* emerg  = (nf->type == "emergency") ? nf : qf;
            Flight* normal = (nf->type == "normal")    ? nf : qf;
            normal->eta    = emerg->eta + DELAY_BUMP;
            normal->status = "delayed";
            sortEDF(flights, rwy);  // queue resort karo
            return true;
        }
    }
    return false;
}
