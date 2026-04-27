#!/bin/bash

# Test Qt6 build with optional QtRest support (moved)
set -euo pipefail

QT_PREFIX=${QT_PREFIX:-/mnt/Builder/bin/Linux/Qt/6.9.2/gcc_64}
BUILD_DIR=${BUILD_DIR:-build_qt6_qtrest}
CLEAN=${CLEAN:-0}

usage() {
    cat <<EOF
Usage: test_qt6_qtrest.sh [--clean] [--qt-prefix PATH] [--build-dir DIR]
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean) CLEAN=1; shift ;;
        --qt-prefix) QT_PREFIX="$2"; shift 2 ;;
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) break ;;
    esac
done

echo "=== Testing Qt6 build with QtRest support ==="

if [ "$CLEAN" -eq 1 ]; then
    echo "Cleaning ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
fi

echo "Configuring: prefix=${QT_PREFIX}, build=${BUILD_DIR}"
cmake -S . -B "${BUILD_DIR}" -DUSE_QT6=ON -DCMAKE_PREFIX_PATH="${QT_PREFIX}"

echo "Building..."
cmake --build "${BUILD_DIR}" -j || { echo "Build failed"; exit 1; }

echo "Checking artifacts..."
if [ -f "${BUILD_DIR}/lib/libqtrest_lib.so" ] || [ -d "${BUILD_DIR}/lib" -a -n "$(ls -A "${BUILD_DIR}/lib" 2>/dev/null)" ]; then
    echo "QtRest library appears present (check ${BUILD_DIR}/lib)"
else
    echo "QtRest library not found in ${BUILD_DIR}/lib"
fi

echo "Plugins in ${BUILD_DIR}/bin (if present):"
ls -la "${BUILD_DIR}/bin" || echo "No bin dir"

echo "=== Qt6 QtRest test completed ==="
