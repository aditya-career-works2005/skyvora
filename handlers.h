#pragma once
// handlers.h -- Flight Runway Management System
// Har pipe command ke liye ek function declare kiya hai yahan.

#include "structs.h"
#include <vector>
#include <string>

void doAddFlight      (State& s, const std::vector<std::string>& args);
void doDeleteFlight   (State& s, const std::vector<std::string>& args);
void doAddRunway      (State& s, const std::vector<std::string>& args);
void doDeleteRunway   (State& s, const std::vector<std::string>& args);
void doMaintenance    (State& s, const std::vector<std::string>& args);
void doAllocate       (State& s, const std::vector<std::string>& args);
void doClear          (State& s, const std::vector<std::string>& args);
void doSetDelay       (State& s, const std::vector<std::string>& args);
void doStatus         (State& s);
void doHistoryFlights (State& s);
void doAutoAllocateAll(State& s);  // batch auto-allocate -- saare pending flights ko ek saath
