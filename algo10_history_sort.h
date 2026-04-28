#pragma once
// algo10_history_sort.h -- Algorithm 10: History Sort by Flight ID
// History vector ko ascending ID order mein sort karta hai -- ye aaise hota hai simple sort

#include "structs.h"
#include <vector>
#include <algorithm>

inline void sortByID(std::vector<Flight>& v) {
    std::sort(v.begin(), v.end(), [](const Flight& a, const Flight& b) {
        return a.id < b.id;
    });
}
