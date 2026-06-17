// handlers.cpp -- Flight Runway Management System
// Har pipe command ke liye ek function hai -- business logic algorithms mein hai, yahan nahi

#include "handlers.h"
#include "algorithms.h"
#include "output.h"
#include <iostream>
#include <algorithm>
#include <string>
using namespace std;

// ADD_FLIGHT  id name type eta departure
void doAddFlight(State& s, const vector<string>& a) {
    if (a.size() < 5) { cout << "ERROR|Missing arguments\n"; return; }

    int id      = stoi(a[0]);
    string name = a[1], type = a[2];
    int eta     = stoi(a[3]), dep = stoi(a[4]);

    // sirf yahi do types allowed hain
    if (type != "normal" && type != "emergency") {
        cout << "ERROR|Type must be normal or emergency\n"; return;
    }

    // duplicate id check -- ek id dobara nahi ho sakta
    for (auto& f : s.flights)
        if (f.id == id) { cout << "ERROR|Flight ID " << id << " already exists\n"; return; }

    if (eta < 0) { cout << "ERROR|ETA must be >= 0\n"; return; }

    // naya flight banao aur dono jagah daalo -- active list aur history mein
    Flight f{id, name, type, eta, dep, -1, "pending", true};
    s.flights.push_back(f);
    s.flight_history.push_back(f);

    cout << "OK|Flight " << name << " (ID:" << id << ") added -- ETA " << eta << " min -- " << type << "\n";
}

// DELETE_FLIGHT  id
void doDeleteFlight(State& s, const vector<string>& a) {
    if (a.empty()) { cout << "ERROR|Missing flight ID\n"; return; }

    int id = stoi(a[0]);
    for (auto& f : s.flights) {
        if (f.id == id && f.active) {
            // agar runway pe hai toh runway ko bhi update karo
            if (f.runway_id != -1) {
                Runway* r = findRunway(s.runways, f.runway_id);
                if (r) {
                    if (r->current_flight_id == id) {
                        // is flight ki jagah queue ka agla flight le lega
                        r->current_flight_id = -1;
                        if (!r->wait_queue.empty()) {
                            int nid = r->wait_queue.front();
                            r->wait_queue.erase(r->wait_queue.begin());
                            r->current_flight_id = nid;
                            Flight* nf = findFlight(s.flights, nid);
                            if (nf) nf->status = "allocated";
                        } else {
                            r->available = true;  // koi nahi hai queue mein -- runway free
                        }
                    } else {
                        // flight queue mein thi -- hata do
                        r->wait_queue.erase(
                            remove(r->wait_queue.begin(), r->wait_queue.end(), id),
                            r->wait_queue.end());
                    }
                }
            }
            // flight ko inactive mark karo
            f.active = false;
            f.status = "departed";
            f.runway_id = -1;
            cout << "OK|Flight " << f.name << " removed\n";
            return;
        }
    }
    cout << "ERROR|Flight not found\n";
}

// ADD_RUNWAY  id name
void doAddRunway(State& s, const vector<string>& a) {
    if (a.size() < 2) { cout << "ERROR|Missing arguments\n"; return; }

    if ((int)s.runways.size() >= MAX_RUNWAYS) {
        cout << "ERROR|Maximum " << MAX_RUNWAYS << " runways allowed\n"; return;
    }

    int id = stoi(a[0]);
    string name = a[1];

    // duplicate runway id nahi chalega
    for (auto& r : s.runways)
        if (r.id == id) { cout << "ERROR|Runway ID " << id << " already exists\n"; return; }

    Runway r{id, name, true, false, -1, {}};
    s.runways.push_back(r);
    cout << "OK|Runway " << name << " (ID:" << id << ") added\n";
}

// DELETE_RUNWAY  id
void doDeleteRunway(State& s, const vector<string>& a) {
    if (a.empty()) { cout << "ERROR|Missing runway ID\n"; return; }

    int id = stoi(a[0]);
    for (auto it = s.runways.begin(); it != s.runways.end(); ++it) {
        if (it->id == id) {
            // busy runway delete nahi kar sakte -- pehle clear karo
            if (!it->available) { cout << "ERROR|Runway is busy -- clear it first\n"; return; }
            cout << "OK|Runway " << it->name << " deleted\n";
            s.runways.erase(it);
            return;
        }
    }
    cout << "ERROR|Runway not found\n";
}

// MAINTENANCE  runway_id
// maintenance toggle karta hai -- on tha toh off, off tha toh on
void doMaintenance(State& s, const vector<string>& a) {
    if (a.empty()) { cout << "ERROR|Missing runway ID\n"; return; }

    Runway* r = findRunway(s.runways, stoi(a[0]));
    if (!r) { cout << "ERROR|Runway not found\n"; return; }

    r->maintenance = !r->maintenance;
    cout << "OK|Maintenance " << (r->maintenance ? "ON" : "OFF") << " for " << r->name << "\n";
}

// Internal helper: ek flight ko ek specific runway pe allocate karo
// "OK|..." ya "ERROR|..." string return karta hai -- print nahi karta, caller print karega
static string allocateOnRunway(State& s, Flight* flight, Runway* runway, const string& mode) {
    string conflict_msg;
    bool preempted = false, queued = false;

    // Emergency flight hai aur runway busy hai -- preempt try karo
    if (flight->type == "emergency" && !runway->available) {
        preempted = tryPreempt(s.flights, s.runways, *runway, flight->id);
        if (preempted) {
            // displaced flight ka kya hua -- dhundho aur report karo
            string displaced_outcome;
            for (auto& f : s.flights) {
                if (!f.active) continue;
                if (f.id == flight->id) continue;  // emergency flight skip karo
                Runway* disp_rwy = findRunway(s.runways, f.runway_id);
                if (!disp_rwy) continue;

                // displaced flight free runway pe land ho gayi
                if (f.status == "allocated" && disp_rwy->current_flight_id == f.id
                    && disp_rwy->id != runway->id) {
                    displaced_outcome = " -- displaced flight moved to free runway "
                                        + disp_rwy->name + " [LANDING NOW]";
                    break;
                }

                // displaced flight queue mein gayi
                if (f.status == "queued" && disp_rwy->id != runway->id) {
                    int qp = 0;
                    for (int i = 0; i < (int)disp_rwy->wait_queue.size(); i++)
                        if (disp_rwy->wait_queue[i] == f.id) { qp = i + 1; break; }
                    displaced_outcome = " -- no free runway, displaced flight queued on "
                                        + disp_rwy->name + " pos " + to_string(qp);
                    break;
                }
            }
            conflict_msg = "PREEMPTION|" + flight->name +
                           " (emergency) preempted normal flight on " + runway->name
                           + displaced_outcome;
        }
    }

    if (!preempted) {
        // preemption nahi hua -- normal landing ya queue try karo
        bool landed = tryLand(s.flights, *runway, flight->id);
        queued = !landed;

        if (queued) {
            // queue mein gaya -- conflict check karo aur sort karo
            bool c = resolveConflict(s.flights, *runway, flight->id);
            sortEDF(s.flights, *runway);
            if (c) conflict_msg = "CONFLICT|Time conflict resolved -- normal flight auto-delayed "
                                  + to_string(DELAY_BUMP) + " min";
        }
    }

    // queue mein position nikalo agar queued hai
    int qpos = 0;
    if (queued)
        for (int i = 0; i < (int)runway->wait_queue.size(); i++)
            if (runway->wait_queue[i] == flight->id) { qpos = i + 1; break; }

    // history mein bhi daalo
    s.flight_history.push_back(*flight);

    // result message banao
    string result;
    if (queued)
        result = "OK|" + flight->name + " queued at position " + to_string(qpos) +
                 " on " + runway->name + " [" + flight->type + "] ETA:" +
                 to_string(flight->eta) + "min [" + mode + "]";
    else
        result = "OK|" + flight->name + " -> " + runway->name +
                 " [LANDING NOW] [" + flight->type + "] ETA:" +
                 to_string(flight->eta) + "min [" + mode + "]";

    // agar conflict tha toh us info ko bhi append karo
    if (!conflict_msg.empty()) result += "|" + conflict_msg;
    return result;
}

// ALLOCATE  flight_id runway_id   (runway_id = -1 means AUTO)
void doAllocate(State& s, const vector<string>& a) {
    if (a.size() < 2) { cout << "ERROR|Missing arguments\n"; return; }

    int fid = stoi(a[0]), rid = stoi(a[1]);

    Flight* flight = findFlight(s.flights, fid);
    if (!flight || !flight->active) { cout << "ERROR|Flight not found or inactive\n"; return; }

    // pehle se allocated ya queued hai toh dobara mat karo
    if (flight->status == "allocated" || flight->status == "queued")
        { cout << "ERROR|Flight is already allocated or queued\n"; return; }

    Runway* runway = nullptr;
    string  mode;

    if (rid == -1) {
        // AUTO mode -- engine khud best runway chunega
        runway = pickBest(s.runways, *flight);
        if (!runway) { cout << "ERROR|No usable runways available\n"; return; }
        mode = "AUTO";
    } else {
        // MANUAL mode -- user ne runway specify kiya hai
        runway = findRunway(s.runways, rid);
        if (!runway)              { cout << "ERROR|Runway not found\n"; return; }
        if (runway->maintenance)  { cout << "ERROR|Runway is under maintenance\n"; return; }
        mode = "MANUAL";
    }

    cout << allocateOnRunway(s, flight, runway, mode) << "\n";
}

// AUTO_ALLOCATE_ALL
// Saare pending flights ko ek saath allocate karo -- emergency pehle (Algorithm 2 order)
void doAutoAllocateAll(State& s) {
    if (s.runways.empty()) { cout << "ERROR|No runways available for auto-allocation\n"; return; }

    // pending flights ki list banao
    vector<Flight*> pending;
    for (auto& f : s.flights)
        if (f.active && f.status == "pending") pending.push_back(&f);

    if (pending.empty()) { cout << "OK|No pending flights to allocate\n"; return; }

    // emergency pehle, phir ETA ke order mein sort karo
    sort(pending.begin(), pending.end(), [](Flight* a, Flight* b) {
        if (a->type != b->type) return a->type == "emergency";
        return a->eta < b->eta;
    });

    int allocated = 0, queued_count = 0, failed = 0;
    string log;

    for (Flight* flight : pending) {
        Runway* runway = pickBest(s.runways, *flight);
        if (!runway) { failed++; continue; }  // koi runway nahi mili -- skip

        string result = allocateOnRunway(s, flight, runway, "AUTO");

        if (result.find("LANDING NOW") != string::npos) allocated++;
        else if (result.find("queued") != string::npos)  queued_count++;

        // log mein add karo -- semicolon separator se
        if (!log.empty()) log += ";";
        auto p1 = result.find('|');
        auto p2 = result.find('|', p1 + 1);
        log += (p1 != string::npos ? result.substr(p1 + 1, p2 - p1 - 1) : result);
    }

    cout << "OK|AUTO_ALLOC|allocated=" << allocated
         << " queued=" << queued_count
         << " failed=" << failed
         << "|" << log << "\n";
}

// CLEAR  runway_id
// Current flight ko depart karao aur queue ka agla flight land karao
void doClear(State& s, const vector<string>& a) {
    if (a.empty()) { cout << "ERROR|Missing runway ID\n"; return; }

    Runway* rwy = findRunway(s.runways, stoi(a[0]));
    if (!rwy) { cout << "ERROR|Runway not found\n"; return; }

    if (rwy->available && rwy->current_flight_id == -1) {
        cout << "ERROR|Runway already free\n"; return;
    }

    // current flight ko depart karo
    if (rwy->current_flight_id != -1) {
        Flight* dep = findFlight(s.flights, rwy->current_flight_id);
        if (dep) { dep->status = "departed"; dep->active = false; dep->runway_id = -1; }
    }

    if (!rwy->wait_queue.empty()) {
        // queue mein koi hai -- agla flight land karao
        int nid = rwy->wait_queue.front();
        rwy->wait_queue.erase(rwy->wait_queue.begin());
        Flight* nf = findFlight(s.flights, nid);
        rwy->current_flight_id = nid;
        rwy->available = false;
        if (nf) nf->status = "allocated";
        cout << "OK|" << rwy->name << " cleared -- next: " << (nf ? nf->name : "unknown")
             << " now landing. Queue: " << rwy->wait_queue.size() << " remaining\n";
    } else {
        // queue bhi khali hai -- runway fully free
        rwy->current_flight_id = -1;
        rwy->available = true;
        cout << "OK|" << rwy->name << " is now free. No flights queued\n";
    }
}

// SET_DELAY  flight_id new_eta
// ETA update karo aur phir flight ko best position pe move karo automatically
void doSetDelay(State& s, const vector<string>& a) {
    if (a.size() < 2) { cout << "ERROR|Missing arguments\n"; return; }

    int fid = stoi(a[0]), new_eta = stoi(a[1]);

    Flight* f = findFlight(s.flights, fid);
    if (!f || !f->active) { cout << "ERROR|Flight not found\n"; return; }
    if (new_eta < 0) { cout << "ERROR|Invalid ETA value\n"; return; }

    f->eta = new_eta;
    string orig_status = f->status;
    f->status = "delayed";

    string auto_msg;

    if (f->runway_id != -1) {
        Runway* cur = findRunway(s.runways, f->runway_id);

        // Case 1: flight queue mein wait kar rahi hai (currently land nahi ho rahi)
        if (cur && cur->current_flight_id != fid) {
            // queue se hata do
            cur->wait_queue.erase(
                remove(cur->wait_queue.begin(), cur->wait_queue.end(), fid),
                cur->wait_queue.end());
            f->runway_id = -1;  // temporarily unassign karo

            // free runway dhundho
            Runway* free_rwy = nullptr;
            for (auto& r : s.runways)
                if (!r.maintenance && r.available) { free_rwy = &r; break; }

            if (free_rwy) {
                // lucky -- free runway mili, seedha land karo
                free_rwy->available         = false;
                free_rwy->current_flight_id = fid;
                f->runway_id = free_rwy->id;
                f->status    = "allocated";
                auto_msg = "|MOVED_TO_FREE|" + f->name +
                           " moved from queue -> free runway " + free_rwy->name +
                           " [LANDING NOW]";
            } else {
                // koi free runway nahi -- sabse chhoti queue mein daalo
                Runway* best = nullptr;
                int shortest = INT_MAX;
                for (auto& r : s.runways) {
                    if (r.maintenance) continue;
                    int ql = (int)r.wait_queue.size();
                    if (ql < shortest) { shortest = ql; best = &r; }
                }
                if (best) {
                    best->wait_queue.push_back(fid);
                    f->runway_id = best->id;
                    f->status    = "queued";
                    sortEDF(s.flights, *best);
                    int qpos = 0;
                    for (int i = 0; i < (int)best->wait_queue.size(); i++)
                        if (best->wait_queue[i] == fid) { qpos = i + 1; break; }
                    auto_msg = "|REQUEUED|No free runway -- " + f->name +
                               " re-queued on " + best->name +
                               " at position " + to_string(qpos);
                }
            }
        }
        // Case 2: flight currently land ho rahi hai -- sirf queue resort karo
        else if (cur) {
            sortEDF(s.flights, *cur);
        }
    }
    // Case 3: flight ka abhi koi runway assign nahi tha -- free runway dhundho
    else if (f->runway_id == -1) {
        for (auto& r : s.runways) {
            if (!r.maintenance && r.available) {
                r.available         = false;
                r.current_flight_id = fid;
                f->runway_id = r.id;
                f->status    = "allocated";
                auto_msg = "|AUTO_ALLOC|" + f->name +
                           " auto-allocated to free runway " + r.name;
                break;
            }
        }
    }

    cout << "OK|Flight " << f->name << " rescheduled -> ETA " << new_eta << " min" << auto_msg << "\n";
}

// STATUS
// Saare active flights aur runways ki info bhejo
void doStatus(State& s) {
    cout << "FLIGHTS_BEGIN\n";
    for (auto& f : s.flights) if (f.active) emitFlight(f);
    cout << "FLIGHTS_END\n";
    cout << "RUNWAYS_BEGIN\n";
    for (auto& r : s.runways) emitRunway(r);
    cout << "RUNWAYS_END\n";
}

// HISTORY_FLIGHTS
// ID ke order mein saari flights ki history bhejo
void doHistoryFlights(State& s) {
    sortByID(s.flight_history);
    cout << "HISTORY_BEGIN\n";
    for (auto& f : s.flight_history) emitFlight(f);
    cout << "HISTORY_END\n";
}
