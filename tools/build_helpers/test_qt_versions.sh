#!/bin/bash

# Test script for Qt version detection (moved)
set -euo pipefail

BUILD_DIR=${BUILD_DIR:-test_build}
QT5_PREFIX=${QT5_PREFIX:-/mnt/Builder/bin/Linux/Qt/5.15.2/gcc_64}
QT6_PREFIX=${QT6_PREFIX:-/mnt/Builder/bin/Linux/Qt/6.9.2/gcc_64}

echo "=== Testing Qt Version Detection ==="
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

echo "1. Testing Qt5 with USE_QT6=OFF"
cmake .. -DUSE_QT6=OFF -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="${QT5_PREFIX}"

echo "2. Testing Qt6 with USE_QT6=ON"
cmake .. -DUSE_QT6=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="${QT6_PREFIX}"

echo "3. Testing auto-detection with Qt5 path"
cmake .. -DCMAKE_PREFIX_PATH="${QT5_PREFIX}" -DCMAKE_BUILD_TYPE=Debug

echo "4. Testing auto-detection with Qt6 path"
cmake .. -DCMAKE_PREFIX_PATH="${QT6_PREFIX}" -DCMAKE_BUILD_TYPE=Debug

echo "=== Test completed ==="
