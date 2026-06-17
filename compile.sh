#!/bin/bash
# compile.sh -- C++ engine build karne ka script
# Run this from project root: bash compile.sh

echo "[INFO] Compiling C++ flight engine..."
g++ -O2 -std=c++17 \
    -o backend/flight_engine \
    backend/main.cpp \
    backend/handlers.cpp

if [ $? -eq 0 ]; then
    echo "[OK] Compiled successfully -> backend/flight_engine"
else
    echo "[ERROR] Compilation failed."
    exit 1
fi
