#!/bin/bash
# Daqster Framework — Unified Dual Qt Build Script
# =============================================================================
# Usage:
#   ./scripts/build.sh qt5          # Configure + build with Qt5 (debug, no tests)
#   ./scripts/build.sh qt6          # Configure + build with Qt6 (debug, no tests)
#   ./scripts/build.sh qt5 clean    # Clean + rebuild with Qt5
#   ./scripts/build.sh qt6 clean    # Clean + rebuild with Qt6
#   ./scripts/build.sh clean        # Clean both build dirs
#
# Environment overrides (optional):
#   QT5_PREFIX=/path/to/Qt/5.15.2/gcc_64   Override default Qt5 path
#   QT6_PREFIX=/path/to/Qt/6.9.2/gcc_64    Override default Qt6 path
#   BUILD_TYPE=Release                     CMake build type (default: Debug)
#   BUILD_TESTS=ON|OFF                     Build unit tests (default: OFF)
#   BUILD_TEST_PLUGINS=ON|OFF              Build test plugins (default: OFF)
#   BUILD_GUI=ON|OFF                       Build GUI components (default: ON)
#   VERBOSE_DEPS=ON|OFF                    Show dependency checks (default: OFF)
#   AUTO_INIT_SUBMODULES=ON|OFF            Auto-init git submodules (default: ON)
#   NUM_JOBS=N                             Parallel build jobs (default: nproc)
#
# Examples:
#   # Default: debug build, all plugins, no tests
#   ./scripts/build.sh qt6
#
#   # Release build with tests
#   BUILD_TYPE=Release BUILD_TESTS=ON ./scripts/build.sh qt6
#
#   # Build with test plugins (for CI)
#   BUILD_TEST_PLUGINS=ON ./scripts/build.sh qt6
#
#   # Headless build (no GUI, no widgets)
#   BUILD_GUI=OFF ./scripts/build.sh qt6
#
#   # Verbose dependency resolution
#   VERBOSE_DEPS=ON ./scripts/build.sh qt6
#
# Perf testing (run after build):
#   cd build_qt6 && DISPLAY=:80 ../scripts/check-perf-degradation.sh
#   # or via env:
#   DAQSTER_VIDEO_FILE=tests/data/bars_h265_1080p25_x3.mp4 ./scripts/run_perf_report.sh
#
# Qt version paths (pinned for reproducibility):
#   Qt5: 5.15.2  (LTS, last Qt5 release)
#   Qt6: 6.9.2   (LTS, current Qt6 LTS)
# =============================================================================

set -euo pipefail

# Pinned Qt versions (update when intentionally upgrading)
QT5_PATH="/mnt/Builder/bin/Linux/Qt/5.15.2/gcc_64"
QT6_PATH="/mnt/Builder/bin/Linux/Qt/6.9.2/gcc_64"

# Default CMake options — override via env
BUILD_TYPE="${BUILD_TYPE:-Debug}"
BUILD_TESTS="${BUILD_TESTS:-OFF}"
BUILD_TEST_PLUGINS="${BUILD_TEST_PLUGINS:-OFF}"
BUILD_GUI="${BUILD_GUI:-ON}"
VERBOSE_DEPS="${VERBOSE_DEPS:-OFF}"
AUTO_INIT_SUBMODULES="${AUTO_INIT_SUBMODULES:-ON}"
NUM_JOBS="${NUM_JOBS:-$(nproc)}"

clean_all() {
    echo "Cleaning build_qt5/ and build_qt6/..."
    rm -rf build_qt5 build_qt6
}

build_qt() {
    local QT_VER=$1
    local QT_PATH=$2
    local BUILD_DIR="build_${QT_VER}"

    echo "=== Configuring with ${QT_VER} (${BUILD_TYPE}) ==="
    echo "    Qt path: ${QT_PATH}"
    echo "    Build dir: ${BUILD_DIR}/"
    echo "    Tests: ${BUILD_TESTS}, Test plugins: ${BUILD_TEST_PLUGINS}, GUI: ${BUILD_GUI}"

    cmake -B "${BUILD_DIR}" \
        -DCMAKE_PREFIX_PATH="${QT_PATH}" \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DUSE_QT6=$([ "${QT_VER}" = "qt6" ] && echo ON || echo OFF) \
        -DDAQSTER_BUILD_TESTS="${BUILD_TESTS}" \
        -DDAQSTER_BUILD_TEST_PLUGINS="${BUILD_TEST_PLUGINS}" \
        -DDAQSTER_BUILD_GUI="${BUILD_GUI}" \
        -DDAQSTER_VERBOSE_DEPENDENCIES="${VERBOSE_DEPS}" \
        -DDAQSTER_AUTO_INIT_SUBMODULES="${AUTO_INIT_SUBMODULES}"

    echo ""
    echo "=== Building with ${QT_VER} (${NUM_JOBS} jobs) ==="
    cmake --build "${BUILD_DIR}" -j"${NUM_JOBS}"

    echo ""
    echo "=== Done: ${QT_VER} ==="
    echo "    Binaries: ${BUILD_DIR}/bin/"
    ls -la "${BUILD_DIR}/bin/" 2>/dev/null || true
}

case "${1:-}" in
    qt5)
        [ "${2:-}" = "clean" ] && rm -rf build_qt5
        build_qt "qt5" "${QT5_PREFIX:-$QT5_PATH}"
        ;;
    qt6)
        [ "${2:-}" = "clean" ] && rm -rf build_qt6
        build_qt "qt6" "${QT6_PREFIX:-$QT6_PATH}"
        ;;
    clean)
        clean_all
        ;;
    *)
        echo "Usage: $0 {qt5|qt6} [clean]"
        echo "  qt5       Build with Qt 5.15.2"
        echo "  qt6       Build with Qt 6.9.2"
        echo "  clean     Remove both build directories"
        echo ""
        echo "Environment overrides:"
        echo "  QT5_PREFIX, QT6_PREFIX, BUILD_TYPE (Debug|Release),"
        echo "  BUILD_TESTS (ON|OFF), BUILD_TEST_PLUGINS (ON|OFF),"
        echo "  BUILD_GUI (ON|OFF), VERBOSE_DEPS (ON|OFF), NUM_JOBS"
        exit 1
        ;;
esac