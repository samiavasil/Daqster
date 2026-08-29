# AGENTS.md - Daqster

> **C++17/CMake project** — Qt-based platform for modular application development
> and management (framework + public plugins).

## What This Is

Daqster is a Qt-based platform for modular application development and management.
It supports different application types through a plugin architecture with graceful
shutdown, process management, and auto plugin discovery.

## Build

```bash
cmake -S . -B build          # Qt6 (default / recommended)
cmake --build build -j
cmake -S . -B build -DUSE_QT6=OFF   # Qt5 (compat)
```

- Requires: CMake 3.20+, Qt 5 or 6, C++17 compiler
- Tests: `-DDAQSTER_BUILD_TESTS=ON -DDAQSTER_BUILD_TEST_PLUGINS=ON`, then `ctest --test-dir build`
- Qt6 is auto-detected (`cmake/FindQtVersion.cmake`); `USE_QT6` is only a FORCE cache variable

## Git Workflow

### Commit Message Format
```
<type> #<issue> (REQ-XXX, REQ-YYY): <description>
Types: fix, feat, refactor, docs, test, chore
```
The REQ block references the requirement(s) being implemented AND any related
requirements (parents/dependents).

### Rules
- **Integration branch = `develop` (MANDATORY):** all feature work merges into
  `develop`. `master` is FROZEN — nothing is merged into `master` except releases
  (see Release Process below). Never branch from `master`.
- **Branch per work item (MANDATORY):** create a new branch
  `<type>/<issue>-<short-name>` or `<type>/<REQ>-<short-name>`; never commit
  directly on `develop`.
- **Commit after every meaningful change** — small, atomic, never batch; always push.
- **Changelog maintenance:** keep `CHANGELOG.md` (BG) and `CHANGELOG.en.md` (EN) up to date.

## Release Process

- **Integration branch `develop`, `master` frozen** (as today) — releases are the
  only thing that touches `master`.
- **Release: `develop` → `master` + tag `vX.Y.Z`** (Option A — no release branch).
- **Version:** `VERSION` file (single source of truth) + `scripts/version.sh X.Y.Z`
  (syncs all 15+ locations) + `version-sync.yml` CI drift check.
- **CI:** `release.yml` is tag-triggered (`v*.*.*`) — Qt6 build (Linux + Windows),
  tests, smoke, AppImage/tarball/ZIP, SHA256SUMS, GitHub Release (body from changelog).
- **Docs:** `master` = release docs — GitHub Pages uses UI-based deployment from `master/docs` (`pages.yml` is disabled).
- **Hotfix:** `hotfix/X.Y.(Z+1)` branched from the `vX.Y.Z` tag → fix → merge
  `master` + tag `vX.Y.(Z+1)` → merge `develop`.
- **Full process:** [DevelopmentProcess/RELEASE-PROCESS.md](./DevelopmentProcess/RELEASE-PROCESS.md)

## Requirements-Driven Development (RDD)

- Requirements live in `DevelopmentProcess/requirements/` (`REQ-SW-{FW,APP,PL,BLD}-*`).
- Full process: `DevelopmentProcess/requirements/RDD-PROCESS.md`. Current phase
  state: `DevelopmentProcess/requirements/RDD-STATUS.md`.

## Session Status

Daily session summaries are stored in `DevelopmentProcess/plans/` as
`YYYY-MM-DD-status.md`. On every new session, read the latest status file first.