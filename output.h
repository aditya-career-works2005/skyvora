#pragma once
// output.h -- Flight Runway Management System
// Pipe protocol serialisation helpers -- C++ se Python ko data bhejne ka format yahan hai.

#include "structs.h"
#include <iostream>

// Wire format:
//   Flight  --> id|name|type|eta|departure|runway_id|status|active
//   Runway  --> id|name|available|maintenance|current_flight_id[|Qflight_id...]

// flight ki saari info pipe ke through bhejo -- Python yahi parse karega
inline void emitFlight(const Flight& f) {
    std::cout << f.id     << "|" << f.name       << "|" << f.type      << "|"
              << f.eta    << "|" << f.departure   << "|" << f.runway_id << "|"
              << f.status << "|" << (f.active ? "1" : "0") << "\n";
}

// runway ka status aur uski queue bhi bhejo
inline void emitRunway(const Runway& r) {
    std::cout << r.id   << "|" << r.name                  << "|"
              << (r.available   ? "1" : "0")              << "|"
              << (r.maintenance ? "1" : "0")              << "|"
              << r.current_flight_id;
    // queue ke flights bhi attach karo -- Q prefix se Python ko pata chalta hai
    for (int q : r.wait_queue) std::cout << "|Q" << q;
    std::cout << "\n";
}
