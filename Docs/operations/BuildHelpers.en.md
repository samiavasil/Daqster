[Български](./BuildHelpers.md) | [English](./BuildHelpers.en.md)

Parent: [Operations Topics](./README.md) | [Documentation Index](../index.en.md)

# Build Helper Scripts

The scripts in `tools/build_helpers/` are development utilities for testing build configurations and managing external submodules.

## Overview

| Script | Purpose |
|---|---|
| `manage_upstream.sh` | Manage upstream remote for external libs |
| `debug_control.sh` | Control ptrace for GDB attach |
| `test_qt_versions.sh` | Automated Qt5/Qt6 detection test |
| `test_qt6_qtrest.sh` | Qt6 build test with QtRest |

---

## manage_upstream.sh

Manages the upstream remote for external libraries (`nodeeditor` and `qtrest_lib`), allowing fetch, merge, and synchronization with upstream projects.

**Usage:**
```bash
./tools/build_helpers/manage_upstream.sh [--dry-run] [--yes] <command>
```

**Commands:**

| Command | Description |
|---|---|
| `status` | Show current upstream HEAD and whether new commits are available |
| `fetch` | Fetch new commits from upstream remote |
| `check` | Check how many new commits exist relative to upstream |
| `setup` | Add `upstream` remote if missing |
| `merge <lib\|all>` | Merge upstream changes into submodule; `all` — both libs |
| `cherry-pick <commit>` | Cherry-pick a specific commit from upstream |
| `help` | Show help |

**Options:**

| Option | Description |
|---|---|
| `--dry-run` | Show what would be done without executing |
| `--yes` | Skip confirmation prompts |

**Env override:**
```bash
NODEEDITOR_UPSTREAM=https://github.com/my-fork/nodeeditor.git \
  ./tools/build_helpers/manage_upstream.sh fetch
```

**Example:**
```bash
# Check for new commits
./tools/build_helpers/manage_upstream.sh check

# Merge nodeeditor from upstream
./tools/build_helpers/manage_upstream.sh merge nodeeditor

# Dry-run before merge
./tools/build_helpers/manage_upstream.sh --dry-run merge all
```

---

## debug_control.sh

Controls `kernel.yama.ptrace_scope` to allow GDB remote attach to processes (required for debugging plugins in Daqster).

> Requires `sudo`. The change is temporary (until reboot) unless saved to `/etc/sysctl.d/`.

**Usage:**
```bash
./tools/build_helpers/debug_control.sh <command> [--yes]
```

**Commands:**

| Command | Description |
|---|---|
| `on` / `enable` | Set `ptrace_scope=0` (allow attach) |
| `off` / `disable` | Set `ptrace_scope=1` (restrict attach) |
| `status` | Show current value |
| `restore` | Restore the value from before the last change |
| `help` | Show help |

**Example debug session:**
```bash
# Before debugging
./tools/build_helpers/debug_control.sh on

# ... GDB attach, debug ...

# After debugging
./tools/build_helpers/debug_control.sh restore
```

---

## test_qt_versions.sh

Tests via cmake configure whether Qt version detection works correctly for all scenarios: USE_QT6 flag and auto-detect from CMAKE_PREFIX_PATH.

**Usage:**
```bash
./tools/build_helpers/test_qt_versions.sh
```

**ENV options:**

| Variable | Default | Description |
|---|---|---|
| `BUILD_DIR` | `test_build` | Directory for the test build |
| `QT5_PREFIX` | `/mnt/Builder/bin/Linux/Qt/5.15.2/gcc_64` | Path to Qt5 |
| `QT6_PREFIX` | `/mnt/Builder/bin/Linux/Qt/6.9.2/gcc_64` | Path to Qt6 |

**What it tests:**
1. Qt5 with `USE_QT6=OFF` + Qt5 prefix
2. Qt6 with `USE_QT6=ON` + Qt6 prefix
3. Auto-detect with Qt5 prefix (no USE_QT6)
4. Auto-detect with Qt6 prefix (no USE_QT6)

**Example:**
```bash
QT5_PREFIX=/usr/lib/qt5 ./tools/build_helpers/test_qt_versions.sh
```

---

## test_qt6_qtrest.sh

Tests a Qt6 build and verifies whether `qtrest_lib` artifacts have been produced. Useful after changes to QtRest for Qt6 compatibility.

> Status: QtRest is ported to Qt6 and `qtrest_lib` builds correctly on both Qt5 and Qt6.

**Usage:**
```bash
./tools/build_helpers/test_qt6_qtrest.sh [--clean] [--qt-prefix PATH] [--build-dir DIR]
```

**Options:**

| Option | Default | Description |
|---|---|---|
| `--clean` | off | Delete the build directory before configure |
| `--qt-prefix PATH` | `/mnt/Builder/bin/Linux/Qt/6.9.2/gcc_64` | Path to Qt6 |
| `--build-dir DIR` | `build_qt6_qtrest` | Directory for the build |

**Example:**
```bash
# Clean Qt6 build test
./tools/build_helpers/test_qt6_qtrest.sh --clean

# With a different Qt6 path
./tools/build_helpers/test_qt6_qtrest.sh --qt-prefix /usr/lib/qt6
```
