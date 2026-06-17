#pragma once
// structs.h -- Flight Runway Management System
// Ye file sabhi shared data structures define karti hai -- poora project isko use karta hai.

#include <string>
#include <vector>

// -- Core domain structs -- ye duniya ke do main objects hain --

struct Flight {
    int         id;
    std::string name;
    std::string type;       // "normal" ya "emergency" -- bas yahi do allowed hain
    int         eta;        // estimated time of arrival in minutes
    int         departure;  // -1 agar pata nahi
    int         runway_id;  // -1 agar abhi assign nahi hua
    std::string status;     // pending | allocated | queued | delayed | departed
    bool        active;     // false ho jaata hai jab flight depart ho jaati hai
};

struct Runway {
    int              id;
    std::string      name;
    bool             available;          // true means koi flight nahi hai abhi
    bool             maintenance;        // maintenance chal rahi hai toh allocate mat karo
    int              current_flight_id;  // -1 if khali hai
    std::vector<int> wait_queue;         // waiting flights ki line -- EDF order mein
};

// Activity selection algorithm ke liye -- algo4 use karta hai isko
struct Activity { int id, start, finish; };

// -- Global application state -- sara kuch yahan stored hai --

struct State {
    std::vector<Flight> flights;        // abhi active flights
    std::vector<Flight> flight_history; // history mein saari flights
    std::vector<Runway> runways;        // saare runways
};

// -- Constants -- ye values tune kar sakte ho agar chahiye --

constexpr int SLOT_WINDOW = 3;  // agar do ETAs 3 min ke andar hain toh conflict hai
constexpr int DELAY_BUMP  = 5;  // conflict pe normal flight ko 5 min aage dhakka maaro
constexpr int MAX_RUNWAYS = 8;  // zyada runway allowed nahi hain
