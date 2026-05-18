#!/bin/bash
# Validate both Qt5 and Qt6 environments and build
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
QT5_PREFIX="${QT5_PREFIX:-$HOME/bin/Qt/5.15.2/gcc_64}"
QT6_PREFIX="${QT6_PREFIX:-$HOME/bin/Qt/6.6.3/gcc_64}"
VERBOSE="${VERBOSE:-0}"

RESULTS_FILE="/tmp/daqster_build_validation.results"

usage() {
    cat <<EOF
Usage: validate_builds.sh [OPTIONS]

Validate Daqster builds with both Qt5 and Qt6

Options:
  --qt5-prefix PATH    Qt5 installation prefix (default: \$HOME/bin/Qt/5.15.2/gcc_64)
  --qt6-prefix PATH    Qt6 installation prefix (default: \$HOME/bin/Qt/6.6.3/gcc_64)
  --verbose            Enable verbose output
  -h, --help           Show this help message

Exit Codes:
  0 = Both Qt5 and Qt6 builds succeeded
  1 = Qt5 build failed
  2 = Qt6 build failed
  3 = Both builds failed
EOF
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --qt5-prefix) QT5_PREFIX="$2"; shift 2 ;;
        --qt6-prefix) QT6_PREFIX="$2"; shift 2 ;;
        --verbose) VERBOSE=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1"; usage; exit 1 ;;
    esac
done

cd "$PROJECT_ROOT"

echo "=========================================="
echo "   Daqster Build Validation"
echo "=========================================="

# Initialize results tracking
> "$RESULTS_FILE"

# Function to test build
test_build() {
    local name="$1"
    local prefix="$2"
    local build_dir="$3"
    
    echo ""
    echo "--- Testing $name ---"
    echo "Qt Prefix: $prefix"
    echo "Build Dir: $build_dir"
    
    # Check Qt installation
    if [ ! -d "$prefix" ]; then
        echo "❌ Error: Qt prefix not found at $prefix"
        echo "FAILED: $name - prefix not found" >> "$RESULTS_FILE"
        return 1
    fi
    
    if [ ! -f "$prefix/bin/qmake" ]; then
        echo "❌ Error: qmake not found at $prefix/bin/qmake"
        echo "FAILED: $name - qmake not found" >> "$RESULTS_FILE"
        return 1
    fi
    
    echo "✓ Qt installation found"
    
    # Run build
    CMAKE_ARGS=("-DCMAKE_PREFIX_PATH=$prefix")
    if [ "$VERBOSE" -eq 1 ]; then
        CMAKE_ARGS+=("-DDAQSTER_VERBOSE_DEPENDENCIES=ON")
    fi

    rm -rf "$build_dir"

    if ! cmake -S . -B "$build_dir" "${CMAKE_ARGS[@]}" > /dev/null 2>&1; then
        echo "❌ CMake configure failed for $name"
        echo "FAILED: $name - cmake configure" >> "$RESULTS_FILE"
        return 1
    fi
    
    NUM_JOBS=$(nproc || echo 4)
    if ! cmake --build "$build_dir" -j"$NUM_JOBS" > /dev/null 2>&1; then
        echo "❌ CMake build failed for $name"
        echo "FAILED: $name - cmake build" >> "$RESULTS_FILE"
        return 1
    fi
    
    if [ ! -f "$build_dir/bin/Daqster" ]; then
        echo "❌ Binary not found: $build_dir/bin/Daqster"
        echo "FAILED: $name - binary not found" >> "$RESULTS_FILE"
        return 1
    fi
    
    echo "✓ Build successful: $build_dir/bin/Daqster"
    echo "SUCCESS: $name" >> "$RESULTS_FILE"
    return 0
}

# Test both versions
qt5_status=0
qt6_status=0

test_build "Qt5.15.2" "$QT5_PREFIX" "build-qt5-validate" || qt5_status=1
test_build "Qt6.6.3" "$QT6_PREFIX" "build-qt6-validate" || qt6_status=1

echo ""
echo "=========================================="
echo "   Validation Results"
echo "=========================================="
cat "$RESULTS_FILE"
echo ""

if [ "$qt5_status" -eq 0 ]; then
    echo "✓ Qt5 build: SUCCESS"
else
    echo "✗ Qt5 build: FAILED"
fi

if [ "$qt6_status" -eq 0 ]; then
    echo "✓ Qt6 build: SUCCESS"
else
    echo "✗ Qt6 build: FAILED (expected - limited plugin support)"
fi

# Return appropriate exit code
EXIT_CODE=$((qt5_status * 1 + qt6_status * 2))
exit $EXIT_CODE
