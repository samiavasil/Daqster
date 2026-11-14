#!/bin/bash
# Daqster Framework — Unified Dual Qt Build Script
# =============================================================================
# Source of truth for the configuration is CMakePresets.json (REQ-SW-PL-090):
# `qt6-debug` → build_qt6/, `qt5-debug` → build_qt5/. QtCreator opens the SAME
# presets, so the two never disagree about generator (Ninja), compilers
# (/usr/bin/cc, /usr/bin/c++) or the DAQSTER_* options — that disagreement is
# what caused the full rebuilds.
#   • Машинно различна стойност → CMakeUserPresets.json (не се редактира
#     споделеният CMakePresets.json и не се пипа кешът на build_qtX).
#
# Usage:
#   ./scripts/build.sh qt5          # Configure + build with Qt5 (preset qt5-debug)
#   ./scripts/build.sh qt6          # Configure + build with Qt6 (preset qt6-debug)
#   ./scripts/build.sh qt5 clean    # Clean + rebuild with Qt5
#   ./scripts/build.sh qt6 clean    # Clean + rebuild with Qt6
#   ./scripts/build.sh clean        # Clean both build dirs
#
# Environment overrides (optional):
#   QT5_PREFIX=/path/to/Qt/5.15.2/gcc_64   Override default Qt5 path
#   QT6_PREFIX=/path/to/Qt/6.9.2/gcc_64    Override default Qt6 path
#   BUILD_TYPE=Debug|Release               CMake build type (default: Debug)
#   BUILD_TESTS=ON|OFF                     Build unit tests (default: ON)
#   BUILD_TEST_PLUGINS=ON|OFF              Build test plugins (default: OFF)
#   BUILD_GUI=ON|OFF                       Build GUI components (default: ON)
#   VERBOSE_DEPS=ON|OFF                    Show dependency checks (default: OFF)
#   AUTO_INIT_SUBMODULES=ON|OFF            Auto-init git submodules (default: ON)
#   NUM_JOBS=N                             Parallel build jobs (default: nproc)
#
# ВАЖНО (REQ-SW-PL-090): стойност, която се разминава с пресета (напр.
# BUILD_TESTS=OFF, BUILD_TYPE=Release), НЕ се записва в споделения build dir —
# той е запазен за пресета. Такива билдове отиват в build_qt<N>-custom/, тоест
# кешът, който QtCreator отваря, остава същият.
#
# Examples:
#   # Default: debug build, all plugins, tests ON
#   ./scripts/build.sh qt6
#
#   # Release build in its own dir (preset cache untouched)
#   BUILD_TYPE=Release ./scripts/build.sh qt6
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
# Qt version paths (pinned for reproducibility) — държат се и в
# CMakePresets.json (qt5-debug/qt6-debug → CMAKE_PREFIX_PATH):
#   Qt5: 5.15.2  (LTS, last Qt5 release)
#   Qt6: 6.9.2   (LTS, current Qt6 LTS)
# =============================================================================

set -euo pipefail

# Pinned Qt versions (update when intentionally upgrading)
QT5_PATH="/mnt/Builder/bin/Linux/Qt/5.15.2/gcc_64"
QT6_PATH="/mnt/Builder/bin/Linux/Qt/6.9.2/gcc_64"

# Default CMake options — override via env. Стойностите по подразбиране СА
# стойностите на пресета (CMakePresets.json), за да не се разминават двата входа.
BUILD_TYPE="${BUILD_TYPE:-Debug}"
BUILD_TESTS="${BUILD_TESTS:-ON}"
BUILD_TEST_PLUGINS="${BUILD_TEST_PLUGINS:-OFF}"
BUILD_GUI="${BUILD_GUI:-ON}"
VERBOSE_DEPS="${VERBOSE_DEPS:-OFF}"
AUTO_INIT_SUBMODULES="${AUTO_INIT_SUBMODULES:-ON}"
NUM_JOBS="${NUM_JOBS:-$(nproc)}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_ROOT}"

# build_dir_for <qt5|qt6> <0|1 — има ли отклонение от пресета>
#   Споделеният dir (build_qtX) е запазен за пресета; всичко, което подава
#   различна стойност, отива в собствения си dir.
build_dir_for() {
    local qt_ver="$1" deviation="$2"
    if [ "${deviation}" -ne 0 ]; then
        echo "build_${qt_ver}-custom"
    else
        echo "build_${qt_ver}"
    fi
}

clean_all() {
    echo "Cleaning build_qt5/ and build_qt6/..."
    rm -rf build_qt5 build_qt6 build_qt5-custom build_qt6-custom
}

build_qt() {
    local QT_VER=$1
    local QT_PATH=$2
    local PRESET="${QT_VER}-debug"

    # ── Отклонения от пресета ──────────────────────────────────────────
    local EXTRA=()
    if [ "${BUILD_TYPE}" != "Debug" ]; then
        EXTRA+=(-DCMAKE_BUILD_TYPE="${BUILD_TYPE}")
    fi
    if [ "${BUILD_TESTS}" != "ON" ]; then
        EXTRA+=(-DDAQSTER_BUILD_TESTS="${BUILD_TESTS}")
    fi
    if [ "${BUILD_TEST_PLUGINS}" != "OFF" ]; then
        EXTRA+=(-DDAQSTER_BUILD_TEST_PLUGINS="${BUILD_TEST_PLUGINS}")
    fi
    if [ "${BUILD_GUI}" != "ON" ]; then
        EXTRA+=(-DDAQSTER_BUILD_GUI="${BUILD_GUI}")
    fi
    if [ "${VERBOSE_DEPS}" != "OFF" ]; then
        EXTRA+=(-DDAQSTER_VERBOSE_DEPENDENCIES="${VERBOSE_DEPS}")
    fi
    if [ "${AUTO_INIT_SUBMODULES}" != "ON" ]; then
        EXTRA+=(-DDAQSTER_AUTO_INIT_SUBMODULES="${AUTO_INIT_SUBMODULES}")
    fi

    local DEVIATION=0
    if [ "${#EXTRA[@]}" -gt 0 ]; then
        DEVIATION=1
    fi
    local BUILD_DIR
    BUILD_DIR="$(build_dir_for "${QT_VER}" "${DEVIATION}")"

    echo "=== Configuring with ${QT_VER} (${BUILD_TYPE}) ==="
    echo "    Preset: ${PRESET}$([ "${DEVIATION}" -eq 0 ] && echo "" || echo "  (+ отклонения → отделен dir)")"
    echo "    Qt path: ${QT_PATH}"
    echo "    Build dir: ${BUILD_DIR}/"
    echo "    Tests: ${BUILD_TESTS}, Test plugins: ${BUILD_TEST_PLUGINS}, GUI: ${BUILD_GUI}"
    if [ "${DEVIATION}" -ne 0 ]; then
        echo "    Забележка: споделеният build_${QT_VER}/ е за пресета ${PRESET};"
        echo "               това е отделен dir, за да не се сменя неговият кеш."
    fi

    # Пресетът изисква Ninja; стар кеш от Makefiles се разпознава с ясно
    # съобщение вместо суровата грешка на CMake за несъвместим генератор.
    if [ -f "${BUILD_DIR}/CMakeCache.txt" ] \
        && ! grep -q '^CMAKE_GENERATOR:INTERNAL=Ninja$' "${BUILD_DIR}/CMakeCache.txt"; then
        echo "ГРЕШКА: ${BUILD_DIR}/ е конфигуриран с друг генератор (не Ninja)." >&2
        echo "        Пуснете: ./scripts/build.sh ${QT_VER} clean" >&2
        exit 1
    fi

    cmake --preset "${PRESET}" -B "${BUILD_DIR}" \
        -DCMAKE_PREFIX_PATH="${QT_PATH}" \
        "${EXTRA[@]}"

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
        [ "${2:-}" = "clean" ] && rm -rf build_qt5 build_qt5-custom
        build_qt "qt5" "${QT5_PREFIX:-$QT5_PATH}"
        ;;
    qt6)
        [ "${2:-}" = "clean" ] && rm -rf build_qt6 build_qt6-custom
        build_qt "qt6" "${QT6_PREFIX:-$QT6_PATH}"
        ;;
    clean)
        clean_all
        ;;
    *)
        echo "Usage: $0 {qt5|qt6} [clean]"
        echo "  qt5       Build with Qt 5.15.2 (preset qt5-debug)"
        echo "  qt6       Build with Qt 6.9.2 (preset qt6-debug)"
        echo "  clean     Remove both build directories"
        echo ""
        echo "Environment overrides:"
        echo "  QT5_PREFIX, QT6_PREFIX, BUILD_TYPE (Debug|Release),"
        echo "  BUILD_TESTS (ON|OFF), BUILD_TEST_PLUGINS (ON|OFF),"
        echo "  BUILD_GUI (ON|OFF), VERBOSE_DEPS (ON|OFF), NUM_JOBS"
        exit 1
        ;;
esac
