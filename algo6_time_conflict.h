#pragma once
// algo6_time_conflict.h -- Algorithm 6: Time-Conflict Detection
// Do ETAs agar SLOT_WINDOW minutes ke andar hain toh conflict hai -- ye aaise hota hai collision check

#include "structs.h"

// agar ETAs 'a' aur 'b' SLOT_WINDOW minutes ke andar hain toh true return karo
inline bool timeConflict(int a, int b) {
    int d = a - b;
    // difference -3 se +3 ke beech mein hai toh conflict hai
    return d >= -SLOT_WINDOW && d <= SLOT_WINDOW;
}
