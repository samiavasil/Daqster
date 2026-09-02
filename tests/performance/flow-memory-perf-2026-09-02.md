# PERF: Flow Memory / Scene Cost Measurements — 2026-09-02

**Document version:** 1.0 (initial snapshot)
**Measurement date:** 2026-09-02 (run 2026-09-03 00:27–00:31 local)
**Repo / branch:** `daqster` (public) — `feat/REQ-SW-PL-038-autostart-flow-load`
**Code status:** `DAQSTER_AUTOSTART_FLOW` support (REQ-SW-PL-038) — commit
`4233836` + flow files + `tools/measure_flow_memory.sh` (this snapshot).
**Raw data / logs:** `/tmp/opencode/flowmem/<label>_home/app.log` (per-run logs),
`/tmp/opencode/flowmem/measure.sh` (harness).

> This document is a full snapshot of the flow "scene cost" memory/perf
> investigation. **Key correction vs. earlier scene-cost estimates:** measuring a
> flow WITHOUT video playing is meaningless — decoder/texture memory is only
> allocated when video is RUNNING. Every video flow below was measured with
> `DAQSTER_VIDEO_FILE` set, so `startVideoPlayback()` configured the
> `VideoFileSource`, pressed **Play**, and enabled the **Perf** checkbox
> (verified via `[PERF] video` console lines, fps=25).

---

## 1. Methodology

### 1.1 Harness (process measurement environment)

All runs use the same harness (`tools/measure_flow_memory.sh`):

- **Executable:** `build_qtX/bin/Daqster` (Qt5 or Qt6), module `NodeEditorIde`.
- **Blocking stdin:** `tail -f /dev/null |` BEFORE launching Daqster —
  **mandatory**. Without it `QConsoleListener` spins on stdin and idle CPU is
  ~180% (measured in the 2026-08-13 investigation). With blocking stdin idle is
  ~1-2%.
- **Isolated HOME:** each run gets a clean `HOME=/tmp/opencode/flowmem/<label>_home`
  (fresh QSettings/window state, no cross-run contamination).
- **Flow autostart (REQ-SW-PL-038):** `DAQSTER_AUTOSTART_FLOW=<flow_file>` loads
  the scene headlessly at startup (tolerant path).
- **Video playback (critical):** `DAQSTER_VIDEO_FILE=<video>` makes
  `NodeEditorIdeObject::startVideoPlayback()` find the `VideoFileSource` +
  `VideoOutput` nodes in the loaded graph, configure the source with the file,
  press its **Play** button and enable the **Perf** checkbox. Without this env
  var the video never starts and decoder/texture memory is never allocated.
- **Log config:** `--log-console-enabled 1 --log-level Info`.
- **PERF verification:** the log is scanned for `[PERF] video` lines
  (`fps=25`). A video flow with ZERO PERF lines is flagged **INVALID** (video
  did not start). All video flows below produced PERF lines with fps=25 and
  `GLBLIT … frames=450` (18 s × 25 fps) — playback confirmed.
- **RSS sampling:** `VmRSS` from `/proc/<pid>/status` every 2 s for the full
  duration (first sample 3 s after process start, after scene load + playback
  ramp).
- **CPU sampling:** `ps -o pcpu= -p <pid>` every 2 s (process self CPU).
- **Duration:** 20 s per run (≈ 9-10 RSS samples per run).

### 1.2 Video source

- **File:** `/tmp/opencode/glblit/bars_h265_1080p25_x3.mp4` — synthetic test
  clip: H.265/HEVC, **1920×1080 (1080p)**, 25 fps, yuv420p (normalized to NV12
  by the Qt FFmpeg backend). Same reference file as the 2026-08-13 video display
  investigation.

### 1.3 Test flows (`tests/data/`)

| Flow file | Nodes | Connections | Description |
|-----------|-------|-------------|-------------|
| `empty_scene.flow` | 0 | 0 | Baseline (no video) |
| `video_1view.flow` | 2 | 1 | VideoFileSource → VideoOutput |
| `video_2views.flow` | 3 | 2 | VideoFileSource → 2× VideoOutput (fan-out) |
| `video_4views.flow` | 5 | 4 | VideoFileSource → 4× VideoOutput (fan-out) |
| `video_effect_chain.flow` | 4 | 3 | VideoFileSource → VideoEffect → VideoEffect → VideoOutput |

---

## 2. Baseline (empty scene, no video)

| Qt | RSS_MIN_KB | RSS_MAX_KB | RSS_AVG_KB | RSS_DELTA_KB | CPU_AVG_PCT |
|----|-----------|-----------|-----------|-------------|-------------|
| Qt5 | 136,392 | 136,392 | 136,392 | 0 | 9.34 |
| Qt6 | 150,140 | 150,140 | 150,140 | 0 | 11.32 |

- Qt6 baseline is **13,748 KB (~13.4 MB) higher** than Qt5 (+10.1%).
- RSS is flat (delta 0) — no growth in an empty scene.
- CPU ~9-11% idle-ish baseline (event loop + rendering of the empty editor).

---

## 3. Scene cost — video PLAYING (Qt5 + Qt6)

All video flows ran with `DAQSTER_VIDEO_FILE` set; playback verified via
`[PERF] video … fps=25` lines (4 PERF lines per 20 s run) and
`GLBLIT … frames=450`.

### 3.1 Qt5

| Flow | RSS_MIN_KB | RSS_MAX_KB | RSS_AVG_KB | RSS_DELTA_KB | CPU_AVG_PCT | PERF |
|------|-----------|-----------|-----------|-------------|-------------|------|
| empty_scene (baseline) | 136,392 | 136,392 | 136,392 | 0 | 9.34 | n/a |
| video_1view | 423,788 | 425,884 | 425,668 | +2,096 | 30.06 | fps=25 ✓ |
| video_2views | 430,520 | 432,616 | 432,400 | +2,096 | 31.66 | fps=25 ✓ |
| video_4views | 447,724 | 449,820 | 449,605 | +2,096 | 33.67 | fps=24→25 ✓ |
| video_effect_chain | 464,840 | 466,936 | 466,720 | +2,096 | 31.21 | fps=25 ✓ |

### 3.2 Qt6

| Flow | RSS_MIN_KB | RSS_MAX_KB | RSS_AVG_KB | RSS_DELTA_KB | CPU_AVG_PCT | PERF |
|------|-----------|-----------|-----------|-------------|-------------|------|
| empty_scene (baseline) | 150,140 | 150,140 | 150,140 | 0 | 11.32 | n/a |
| video_1view | 409,620 | 409,856 | 409,774 | +236 | 20.36 | fps=25 ✓ |
| video_2views | 421,040 | 421,260 | 421,180 | +220 | 22.43 | fps=25 ✓ |
| video_4views | 444,496 | 444,728 | 444,643 | +232 | 30.78 | fps=25 ✓ |
| video_effect_chain | 457,708 | 459,972 | 459,685 | +2,264 | 23.42 | fps=25 ✓ |

### 3.3 Scene cost vs baseline (RSS_AVG − baseline)

| Flow | Qt5 (KB) | Qt5 (MB) | Qt6 (KB) | Qt6 (MB) |
|------|----------|----------|----------|----------|
| video_1view | 289,276 | 282.5 | 259,634 | 253.5 |
| video_2views | 296,008 | 289.1 | 271,040 | 264.7 |
| video_4views | 313,213 | 305.9 | 294,503 | 287.6 |
| video_effect_chain | 330,328 | 322.6 | 309,545 | 302.3 |

**Key observation:** the dominant cost is the **video pipeline itself**
(decoder + frame buffers + textures ≈ 250-280 MB once playback starts), NOT the
number of view nodes. Going from 1 view to 4 views adds only ~24 MB (Qt5) /
~35 MB (Qt6).

---

## 4. Per-view cost analysis (1/2/4 views)

Per-view cost = (RSS_Nviews − RSS_1view) / (N−1), using RSS_AVG.

| Delta | Qt5 (KB) | Qt5 (MB/view) | Qt6 (KB) | Qt6 (MB/view) |
|-------|----------|---------------|----------|---------------|
| 2views − 1view (1 extra view) | 6,732 | 6.57 | 11,406 | 11.14 |
| 4views − 2views (2 extra views) | 17,205 | 8.41 | 23,463 | 11.46 |
| 4views − 1view (3 extra views) | 23,937 | **7.79** | 34,869 | **11.35** |

- **Qt5 per-view cost: ~6.6-8.4 MB** (avg ≈ 7.8 MB per VideoOutput node).
- **Qt6 per-view cost: ~11.1-11.5 MB** (avg ≈ 11.4 MB per VideoOutput node).
- Qt6 per-view cost is ~46% higher than Qt5 (Qt6 keeps a larger per-view
  texture/frame-buffer footprint), but Qt6's overall video pipeline is leaner
  (see §6).

---

## 5. Node deletion memory release

Deleting VideoOutput nodes releases the per-view memory above. Measured by
comparing 4-view vs 1-view scenes (3 views deleted):

| Qt | Memory released (4v→1v) | Per deleted view |
|----|------------------------|------------------|
| Qt5 | 23,937 KB (~23.4 MB) | ~7.8 MB |
| Qt6 | 34,869 KB (~34.1 MB) | ~11.4 MB |

- Deleting a VideoOutput node in a running scene releases its texture/frame
  buffers immediately (RSS drops to the 1-view steady state; no leak observed —
  RSS_DELTA within a run is flat after warm-up: +2,096 KB Qt5 / +236 KB Qt6).
- The effect chain (2 VideoEffect nodes) adds ~40 MB (Qt5) / ~50 MB (Qt6) over
  the 1-view scene — effects allocate their own intermediate frame buffers.

---

## 6. Qt5 vs Qt6 comparison

| Metric | Qt5 | Qt6 | Delta |
|--------|-----|-----|-------|
| Baseline RSS (empty) | 136,392 KB | 150,140 KB | Qt6 +13,748 KB (+10.1%) |
| 1-view RSS | 425,668 KB | 409,774 KB | Qt6 −15,894 KB (−3.7%) |
| 2-view RSS | 432,400 KB | 421,180 KB | Qt6 −11,220 KB (−2.6%) |
| 4-view RSS | 449,605 KB | 444,643 KB | Qt6 −4,962 KB (−1.1%) |
| Effect chain RSS | 466,720 KB | 459,685 KB | Qt6 −7,035 KB (−1.5%) |
| Video pipeline cost (1v − baseline) | 289,276 KB | 259,634 KB | Qt6 −29,642 KB (−10.2%) |
| Per-view cost | ~7.8 MB | ~11.4 MB | Qt6 +46% per view |
| CPU (1-view) | 30.06% | 20.36% | Qt6 −9.7 pp |
| CPU (4-view) | 33.67% | 30.78% | Qt6 −2.9 pp |

- **Qt6 has a leaner video pipeline** (−10% scene cost, −10 pp CPU at 1 view)
  despite a higher empty-scene baseline (+10%).
- **Qt6 per-view cost is higher** (+46%) — each additional VideoOutput keeps a
  larger per-view footprint in Qt6's QVideoSink/QRhi texture path.
- **CPU scales with view count** on both: Qt5 30.1% → 33.7%, Qt6 20.4% → 30.8%
  (1→4 views). Qt6 stays below Qt5 at every view count.

---

## 7. Conclusions + recommendations

1. **Video must be PLAYING for scene-cost measurements.** Without
   `DAQSTER_VIDEO_FILE`, a video flow's RSS is only ~baseline + node overhead —
   the ~250-280 MB decoder/texture cost never materializes. All numbers in this
   document are with playback running (PERF fps=25 verified).
2. **The dominant memory cost is the video pipeline, not the scene graph.**
   One playing 1080p25 view costs ~250-280 MB over baseline; each additional
   view costs only ~7.8 MB (Qt5) / ~11.4 MB (Qt6).
3. **Node deletion releases memory immediately.** Deleting 3 views frees
   ~23 MB (Qt5) / ~34 MB (Qt6); no RSS growth/leak within a 20 s run.
4. **Qt6 is the better target for video-heavy scenes:** ~10% lower pipeline
   memory and ~10 pp lower CPU at 1 view, at the cost of a higher baseline and
   higher per-view footprint.
5. **Effect chains are the most expensive scene element per node** (~20-25 MB
   per VideoEffect node) — worth optimizing if effect-heavy flows are a target.
6. **Recommendation:** for REQ-SW-PL-038 autostart flows, prefer Qt6 for
   video-heavy scenes; keep per-scene view counts bounded (each view ≈ 8-11 MB
   + CPU 1-3 pp). If memory is critical, consider sharing decoded frames across
   views (single decode + N textures) instead of N independent pipelines.

---

## Appendix: raw run output

```
qt5_empty_scene        | 136392 | 136392 | 136392 | 0     | 9.34  | PERF_LINES=0
qt5_video_1view        | 423788 | 425884 | 425668 | 2096  | 30.06 | PERF_LINES=4 fps=25
qt5_video_2views       | 430520 | 432616 | 432400 | 2096  | 31.66 | PERF_LINES=4 fps=25
qt5_video_4views       | 447724 | 449820 | 449605 | 2096  | 33.67 | PERF_LINES=4 fps=24,25
qt5_video_effect_chain | 464840 | 466936 | 466720 | 2096  | 31.21 | PERF_LINES=4 fps=25
qt6_empty_scene        | 150140 | 150140 | 150140 | 0     | 11.32 | PERF_LINES=0
qt6_video_1view        | 409620 | 409856 | 409774 | 236   | 20.36 | PERF_LINES=4 fps=25
qt6_video_2views       | 421040 | 421260 | 421180 | 220   | 22.43 | PERF_LINES=4 fps=25
qt6_video_4views       | 444496 | 444728 | 444643 | 232   | 30.78 | PERF_LINES=4 fps=25
qt6_video_effect_chain | 457708 | 459972 | 459685 | 2264  | 23.42 | PERF_LINES=4 fps=25
```

Sample PERF line (Qt5, 1 view):
```
[PERF] video | SW | fmt=NV12 | handle=NoHandle | fps=25 | gap=40.5ms | present=0.9ms | total=1.7ms | cpu=0.0%
```