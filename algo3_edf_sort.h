#pragma once
// algo3_edf_sort.h -- Algorithm 3: Earliest Deadline First Sort
// Runway ki wait_queue ko EDF order mein sort karta hai
// Emergency pehle, phir ETA ke hisaab se -- ye aaise hota hai scheduling

#include "structs.h"
#include <vector>
#include <algorithm>

// flight dhundho -- har algo file mein apna helper hai to avoid circular includes
inline Flight* findFlight_edf(std::vector<Flight>& v, int id) {
    for (auto& f : v) if (f.id == id) return &f;
    return nullptr;
}

// runway ki wait_queue in-place sort karo EDF order mein
inline void sortEDF(std::vector<Flight>& flights, Runway& rwy) {
    std::sort(rwy.wait_queue.begin(), rwy.wait_queue.end(),
        [&](int a, int b) {
            Flight* fa = findFlight_edf(flights, a);
            Flight* fb = findFlight_edf(flights, b);
            if (!fa || !fb) return false;

            // emergency ko priority 0, normal ko 1 -- chhota number pehle aata hai
            int pa = (fa->type == "emergency") ? 0 : 1;
            int pb = (fb->type == "emergency") ? 0 : 1;

            if (pa != pb) return pa < pb;

            // same type hai toh jo pehle aayega woh pehle jaayega
            return fa->eta < fb->eta;
        });
}
