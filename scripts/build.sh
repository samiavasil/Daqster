#!/bin/bash
# Build Daqster with Qt5 or Qt6
# Usage: ./scripts/build.sh [qt5|qt6] [clean]

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SCRIPT_DIR="${PROJECT_ROOT}/tools/build_helpers"

usage() {
    cat <<EOF
Usage: ./scripts/build.sh [qt5|qt6] [clean]

Arguments:
  qt5       Build with Qt5 (default)
  qt6       Build with Qt6
  clean     Clean build directory before building

Examples:
  ./scripts/build.sh qt5          # Configure + build with Qt5
  ./scripts/build.sh qt6          # Configure + build with Qt6
  ./scripts/build.sh qt5 clean    # Clean + rebuild with Qt5
  ./scripts/build.sh clean        # Clean both build dirs
EOF
}

# Parse arguments
QT_VERSION="qt5"
CLEAN=0

for arg in "$@"; do
    case "$arg" in
        qt5) QT_VERSION="qt5" ;;
        qt6) QT_VERSION="qt6" ;;
        clean) CLEAN=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown argument: $arg"; usage; exit 1 ;;
    esac
done

cd "$PROJECT_ROOT"

if [ "$CLEAN" -eq 1 ] && [ "$QT_VERSION" = "qt5" ]; then
    echo "Cleaning build_qt5..."
    rm -rf build_qt5
elif [ "$CLEAN" -eq 1 ] && [ "$QT_VERSION" = "qt6" ]; then
    echo "Cleaning build_qt6..."
    rm -rf build_qt6
elif [ "$CLEAN" -eq 1 ]; then
    echo "Cleaning both build directories..."
    rm -rf build_qt5 build_qt6
    exit 0
fi

if [ "$QT_VERSION" = "qt5" ]; then
    echo "=== Building Daqster with Qt5 ==="
    QT_PREFIX="${QT_PREFIX:-/usr/lib/x86_64-linux-gnu}"
    BUILD_DIR="build_qt5"
    "${SCRIPT_DIR}/build_qt5.sh" --clean --build-dir "$BUILD_DIR" --qt-prefix "$QT_PREFIX"
else
    echo "=== Building Daqster with Qt6 ==="
    QT_PREFIX="${QT_PREFIX:-/usr/lib/x86_64-linux-gnu/cmake/Qt6}"
    BUILD_DIR="build_qt6"
    "${SCRIPT_DIR}/build_qt6.sh" --clean --build-dir "$BUILD_DIR" --qt-prefix "$QT_PREFIX"
fi