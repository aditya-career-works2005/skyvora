// main.cpp -- Flight Runway Management System
// Ye program ka entry point hai, stdin se commands padh ke handlers ko bhejta hai.
//
// Pipe protocol -- ek line mein ek command:
//   ADD_FLIGHT       id name type eta departure
//   DELETE_FLIGHT    id
//   ADD_RUNWAY       id name
//   DELETE_RUNWAY    id
//   MAINTENANCE      runway_id
//   ALLOCATE         flight_id runway_id   (runway_id=-1 means AUTO for any type)
//   AUTO_ALLOCATE_ALL                      (saare pending flights ek saath allocate karo)
//   CLEAR            runway_id
//   SET_DELAY        flight_id new_eta
//   STATUS
//   HISTORY_FLIGHTS

#include "handlers.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

int main() {
    // ye sync band karo warna bahut slow ho jaata hai
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    State s;
    string line;

    // jab tak stdin mein lines aati rahein, process karte raho
    while (getline(cin, line)) {
        if (line.empty()) continue;

        // line ko command aur arguments mein tod do
        istringstream iss(line);
        string cmd;
        iss >> cmd;

        vector<string> args;
        string tok;
        while (iss >> tok) args.push_back(tok);

        // command dekh ke sahi handler ko call karo -- ye aaise hota hai dispatch
        if      (cmd == "ADD_FLIGHT")        doAddFlight(s, args);
        else if (cmd == "DELETE_FLIGHT")     doDeleteFlight(s, args);
        else if (cmd == "ADD_RUNWAY")        doAddRunway(s, args);
        else if (cmd == "DELETE_RUNWAY")     doDeleteRunway(s, args);
        else if (cmd == "MAINTENANCE")       doMaintenance(s, args);
        else if (cmd == "ALLOCATE")          doAllocate(s, args);
        else if (cmd == "AUTO_ALLOCATE_ALL") doAutoAllocateAll(s);
        else if (cmd == "CLEAR")             doClear(s, args);
        else if (cmd == "SET_DELAY")         doSetDelay(s, args);
        else if (cmd == "STATUS")            doStatus(s);
        else if (cmd == "HISTORY_FLIGHTS")   doHistoryFlights(s);
        else cout << "ERROR|Unknown command: " << cmd << "\n";

        // turant bhejo, buffer mein mat rakho
        cout << flush;
    }
    return 0;
}
