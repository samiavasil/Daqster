#!/bin/bash
# Build Daqster with Qt6
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
QT_PREFIX="${QT_PREFIX:-$HOME/bin/Qt/6.6.3/gcc_64}"
BUILD_DIR="${BUILD_DIR:-build-qt6-checkpoint}"
CLEAN="${CLEAN:-0}"
VERBOSE="${VERBOSE:-0}"

usage() {
    cat <<EOF
Usage: build_qt6.sh [OPTIONS]

Options:
  --clean              Clean build directory before building
  --qt-prefix PATH     Qt6 installation prefix (default: \$HOME/bin/Qt/6.6.3/gcc_64)
  --build-dir DIR      Build directory name (default: build-qt6-checkpoint)
  --verbose            Enable verbose output
  -h, --help           Show this help message

Example:
  ./build_qt6.sh --clean --verbose
EOF
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean) CLEAN=1; shift ;;
        --qt-prefix) QT_PREFIX="$2"; shift 2 ;;
        --build-dir) BUILD_DIR="$2"; shift 2 ;;
        --verbose) VERBOSE=1; shift ;;
        -h, --help) usage; exit 0 ;;
        *) echo "Unknown option: $1"; usage; exit 1 ;;
    esac
done

cd "$PROJECT_ROOT"

echo "=== Building Daqster with Qt6 ==="
echo "Qt Prefix: $QT_PREFIX"
echo "Build Dir: $BUILD_DIR"
echo "Project Root: $PROJECT_ROOT"

if [ "$CLEAN" -eq 1 ]; then
    echo "Cleaning $BUILD_DIR..."
    rm -rf "$BUILD_DIR"
fi

echo "Configuring with CMake..."
CMAKE_ARGS="-DCMAKE_PREFIX_PATH=$QT_PREFIX"
if [ "$VERBOSE" -eq 1 ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DDAQSTER_VERBOSE_DEPENDENCIES=ON"
fi

cmake -S . -B "$BUILD_DIR" $CMAKE_ARGS

echo "Building..."
NUM_JOBS=$(nproc || echo 4)
cmake --build "$BUILD_DIR" -j"$NUM_JOBS" || {
    echo "Build failed! Qt6 may have limited plugin support."
    echo "Check CMakeOutput.log for details."
    exit 1
}

echo "=== Qt6 build completed successfully ==="
echo "Binary: $BUILD_DIR/bin/Daqster"
echo ""
echo "To run QtCoinTrader plugin:"
echo "  cd $BUILD_DIR/bin"
echo "  ./Daqster QtCoinTrader"
