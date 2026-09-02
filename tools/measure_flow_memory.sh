#!/usr/bin/env bash
# measure_flow_memory.sh — RSS/CPU measurement for NodeEditor flow scenes
# WITH VIDEO PLAYING (REQ-SW-PL-038).
#
# Usage:
#   tools/measure_flow_memory.sh <qt5|qt6> <flow_file> <label> <duration_s>
#
# Launches the Daqster NodeEditorIde with:
#   DAQSTER_AUTOSTART_FLOW=<flow_file>   (loads the scene headlessly)
#   DAQSTER_VIDEO_FILE=<video>           (starts video playback + Perf)
# samples RSS (VmRSS from /proc/<pid>/status) and CPU (ps pcpu) every 2 s,
# then kills the app and prints:
#   LABEL | RSS_MIN_KB | RSS_MAX_KB | RSS_AVG_KB | RSS_DELTA_KB | CPU_AVG_PCT | PERF_LINES | PERF_FPS
#
# PERF verification: the log is scanned for `[PERF] video` lines (fps=25).
# If a video flow produced NO PERF lines, the video did not start and the
# measurement is INVALID — the script prints PERF_LINES=0 and exits 3.
#
# Blocking stdin via `tail -f /dev/null |` is MANDATORY — without it the app
# spins at ~180% idle CPU.
set -u

if [ $# -ne 4 ]; then
    echo "Usage: $0 <qt5|qt6> <flow_file> <label> <duration_s>" >&2
    exit 2
fi

QT_VER="$1"
FLOW_FILE="$2"
LABEL="$3"
DURATION_S="$4"

# Test video file (HEVC 1920x1080 25fps, ~3x looped bars pattern). Generated
# with ffmpeg; see tests/performance/flow-memory-perf-2026-09-02.md.
VIDEO_FILE="/tmp/opencode/glblit/bars_h265_1080p25_x3.mp4"
if [ ! -f "$VIDEO_FILE" ]; then
    echo "ERROR: video file not found: $VIDEO_FILE" >&2
    exit 2
fi

case "$QT_VER" in
    qt5) QT_ROOT="/mnt/Builder/bin/Linux/Qt/5.15.2/gcc_64" ;;
    qt6) QT_ROOT="/mnt/Builder/bin/Linux/Qt/6.9.2/gcc_64" ;;
    *) echo "ERROR: qt version must be qt5 or qt6, got '$QT_VER'" >&2; exit 2 ;;
esac

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
APP="$REPO_DIR/build_qt${QT_VER#qt}/bin/Daqster"
BUILD_BIN="$REPO_DIR/build_qt${QT_VER#qt}/bin"

if [ ! -x "$APP" ]; then
    echo "ERROR: app not found: $APP" >&2
    exit 2
fi
if [ ! -f "$FLOW_FILE" ]; then
    echo "ERROR: flow file not found: $FLOW_FILE" >&2
    exit 2
fi

HOME_DIR="/tmp/opencode/flowmem/${LABEL}_home"
mkdir -p "$HOME_DIR"
LOG_FILE="$HOME_DIR/app.log"
: > "$LOG_FILE"

# Launch the app with stdin blocked (tail -f /dev/null |). $! is the PID of
# the last process in the background pipeline — the app itself.
tail -f /dev/null | env \
    HOME="$HOME_DIR" \
    DISPLAY="${DISPLAY:-:0}" \
    LD_LIBRARY_PATH="$QT_ROOT/lib:$BUILD_BIN" \
    DAQSTER_AUTOSTART_FLOW="$FLOW_FILE" \
    DAQSTER_VIDEO_FILE="$VIDEO_FILE" \
    "$APP" NodeEditorIde --log-console-enabled 1 --log-level Info \
    >"$LOG_FILE" 2>&1 &
APP_PID=$!

# Wait for the app to appear in /proc (startup can take a few seconds).
START_WAIT=30
for _ in $(seq 1 "$START_WAIT"); do
    if [ -r "/proc/$APP_PID/status" ]; then
        break
    fi
    sleep 1
done
if [ ! -r "/proc/$APP_PID/status" ]; then
    echo "ERROR: app (pid $APP_PID) did not start within ${START_WAIT}s" >&2
    kill "$APP_PID" 2>/dev/null
    exit 1
fi

# Give the scene a moment to load + playback to start before the first sample.
sleep 3

RSS_SAMPLES=()
CPU_SAMPLES=()
RSS_FIRST=""
RSS_LAST=""

END=$(( $(date +%s) + DURATION_S ))
while [ "$(date +%s)" -lt "$END" ]; do
    if [ ! -r "/proc/$APP_PID/status" ]; then
        echo "WARN: app (pid $APP_PID) exited early" >&2
        break
    fi
    RSS_KB=$(awk '/^VmRSS:/ {print $2}' "/proc/$APP_PID/status")
    CPU_PCT=$(ps -o pcpu= -p "$APP_PID" 2>/dev/null | tr -d ' ')
    if [ -n "$RSS_KB" ]; then
        RSS_SAMPLES+=("$RSS_KB")
        if [ -z "$RSS_FIRST" ]; then RSS_FIRST="$RSS_KB"; fi
        RSS_LAST="$RSS_KB"
    fi
    if [ -n "$CPU_PCT" ]; then
        CPU_SAMPLES+=("$CPU_PCT")
    fi
    sleep 2
done

# Kill the app and the blocking tail.
kill "$APP_PID" 2>/dev/null
sleep 1
kill "$APP_PID" 2>/dev/null
pkill -P "$APP_PID" 2>/dev/null
# Kill the tail feeding stdin (it never exits on its own).
TAIL_PID=$(pgrep -f "tail -f /dev/null" | head -1)
[ -n "$TAIL_PID" ] && kill "$TAIL_PID" 2>/dev/null

if [ "${#RSS_SAMPLES[@]}" -eq 0 ]; then
    echo "ERROR: no RSS samples collected for $LABEL" >&2
    exit 1
fi

# Compute stats.
RSS_MIN="${RSS_SAMPLES[0]}"
RSS_MAX="${RSS_SAMPLES[0]}"
RSS_SUM=0
for v in "${RSS_SAMPLES[@]}"; do
    if [ "$v" -lt "$RSS_MIN" ]; then RSS_MIN="$v"; fi
    if [ "$v" -gt "$RSS_MAX" ]; then RSS_MAX="$v"; fi
    RSS_SUM=$((RSS_SUM + v))
done
RSS_AVG=$((RSS_SUM / ${#RSS_SAMPLES[@]}))
RSS_DELTA=$((RSS_LAST - RSS_FIRST))

CPU_SUM=0
for v in "${CPU_SAMPLES[@]}"; do
    CPU_SUM=$(awk -v a="$CPU_SUM" -v b="$v" 'BEGIN { printf "%.2f", a + b }')
done
CPU_AVG=$(awk -v s="$CPU_SUM" -v n="${#CPU_SAMPLES[@]}" 'BEGIN { printf "%.2f", s / n }')

# PERF verification: count [PERF] video lines and extract fps values.
PERF_LINES=$(grep -c "\[PERF\] video" "$LOG_FILE" 2>/dev/null || true)
PERF_FPS=$(grep -o "fps=[0-9.]*" "$LOG_FILE" 2>/dev/null | sort -u | tr '\n' ',' | sed 's/,$//')

echo "$LABEL | $RSS_MIN | $RSS_MAX | $RSS_AVG | $RSS_DELTA | $CPU_AVG | PERF_LINES=$PERF_LINES | FPS=$PERF_FPS"

# A video flow with zero PERF lines means playback never started → invalid.
if [ "$PERF_LINES" -eq 0 ] && ! grep -q "VideoFileSource" "$FLOW_FILE"; then
    : # empty scene / non-video flow: PERF lines not expected
elif [ "$PERF_LINES" -eq 0 ]; then
    echo "INVALID: no [PERF] lines for video flow $LABEL — video did not start" >&2
    exit 3
fi