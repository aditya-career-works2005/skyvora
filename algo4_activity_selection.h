#pragma once
// algo4_activity_selection.h -- Algorithm 4: Activity Selection (Greedy)
// Maximum non-overlapping flight slots chunne ka greedy algorithm
// Ye classic greedy problem hai -- finish time se sort karo phir greedily lo

#include "structs.h"
#include <algorithm>

// 'now' ke baad se shuru hone wale maximum non-overlapping activities count karo
// Greedy approach: finish time ke hisaab se sort, phir ek ek karke lo
inline int activitySelection(Activity acts[], int n, int now) {
    // pehle finish time se sort karo -- ye greedy ki trick hai
    std::sort(acts, acts + n, [](const Activity& a, const Activity& b) {
        return a.finish < b.finish;
    });

    int cnt = 0;
    int last = now;  // last selected activity ka finish time track karo

    for (int i = 0; i < n; i++) {
        // agar ye activity last ke baad shuru hoti hai toh le lo
        if (acts[i].start >= last) {
            cnt++;
            last = acts[i].finish;
        }
    }
    return cnt;
}
