#!/usr/bin/env bash
# =============================================================================
# build.sh — Linux/macOS One-Click Build & Test Script
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=========================================================="
echo " Building C-Subset Mini-Compiler (Linux/macOS)"
echo "=========================================================="

if command -v cmake >/dev/null 2>&1; then
    echo "[INFO] Using CMake..."
    mkdir -p build
    cmake -B build -S .
    cmake --build build --config Release -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)
elif command -v g++ >/dev/null 2>&1; then
    echo "[INFO] CMake not found. Compiling with g++ -std=c++17 directly..."
    g++ -std=c++17 -O2 -I mini_compiler/include \
        mini_compiler/src/lexer.cpp \
        mini_compiler/src/parser.cpp \
        mini_compiler/src/codegen.cpp \
        mini_compiler/src/vm.cpp \
        mini_compiler/src/main.cpp \
        -o mini_compiler
elif command -v clang++ >/dev/null 2>&1; then
    echo "[INFO] Using clang++ -std=c++17..."
    clang++ -std=c++17 -O2 -I mini_compiler/include \
        mini_compiler/src/lexer.cpp \
        mini_compiler/src/parser.cpp \
        mini_compiler/src/codegen.cpp \
        mini_compiler/src/vm.cpp \
        mini_compiler/src/main.cpp \
        -o mini_compiler
else
    echo "[ERROR] Neither cmake, g++, nor clang++ found in PATH." >&2
    exit 1
fi

echo "[SUCCESS] Build completed successfully."

echo "=========================================================="
echo " Running Automated Verification Testbed via Python"
echo "=========================================================="

if command -v python3 >/dev/null 2>&1; then
    python3 verification/verify_runner.py
elif command -v python >/dev/null 2>&1; then
    python verification/verify_runner.py
else
    echo "[WARNING] Python 3 not detected. Skipping automated tests."
fi
