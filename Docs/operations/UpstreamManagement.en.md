# Upstream Management Guide

Parent: [Operations Topics](./README.md) | [Documentation Index](../index.en.md)

This guide describes how to manage upstream tracking for external libraries used by Daqster.

## External Libraries

### NodeEditor
- Upstream repository: https://github.com/paceholder/nodeeditor
- Your fork: https://github.com/samiavasil/nodeeditor
- Working branch: feature/deembed-hover-fronting-wm
- Integration in Daqster: submodule under src/external_libs/nodeeditor

### QtRest
- Upstream repository: https://github.com/kafeg/qtrest
- Your fork: https://github.com/samiavasil/qtrest
- Working branch: feature/qt6-port-cmake-unified
- Integration in Daqster: submodule under src/external_libs/qtrest_lib/qtrest

## Fork Delta (compared to upstream)

This section documents Daqster-specific changes in both forks.

### NodeEditor: what changed and why

- Added embed/de-embed lifecycle for node widgets.
Reason: Daqster uses richer QWidget-based node panels and needs explicit control over embedded vs detached behavior.

- Added hover fronting behavior for detached windows.
Reason: users need quick window fronting when detached node panels overlap scene items.

- Added guard and lightweight fronting path to avoid unnecessary window reposition/flag churn.
Reason: default upstream behavior did not fully cover window manager edge cases in Daqster workflow.

### QtRest: what changed and why

- Unified CMake C++ standard to C++17.
Reason: Daqster and its plugins use C++17, so this removes mixed-target build incompatibilities.

- Added Qt5/Qt6 dual-major linking and dependency gating in Daqster integration.
Reason: keep one code line that builds in both Qt5 and Qt6.

- Refined conditional inclusion of QtCoinTrader and qtrest targets based on available Qt modules.
Reason: avoid partial or false-positive target enablement when required Qt modules are missing.

## Check current divergence from upstream

Use live git commands instead of static ahead/behind numbers:

```bash
# NodeEditor
cd src/external_libs/nodeeditor
git fetch upstream
git rev-list --left-right --count HEAD...upstream/master

# QtRest
cd src/external_libs/qtrest_lib/qtrest
git fetch upstream
git rev-list --left-right --count HEAD...upstream/master
```

## Upstream Management Script

Use tools/build_helpers/manage_upstream.sh for common operations:

```bash
# Show current status
./tools/build_helpers/manage_upstream.sh status

# Check for updates
./tools/build_helpers/manage_upstream.sh check

# Fetch latest upstream changes
./tools/build_helpers/manage_upstream.sh fetch

# Merge upstream changes
./tools/build_helpers/manage_upstream.sh merge nodeeditor
./tools/build_helpers/manage_upstream.sh merge qtrest
./tools/build_helpers/manage_upstream.sh merge all

# Cherry-pick a specific commit
./tools/build_helpers/manage_upstream.sh cherry-pick <commit-hash>
```

## Workflow For Upstream Updates

### 1. Regular check (monthly)
```bash
./tools/build_helpers/manage_upstream.sh check
./tools/build_helpers/manage_upstream.sh fetch
```

### 2. Analyze incoming changes
```bash
# NodeEditor
cd src/external_libs/nodeeditor
git log HEAD..upstream/master --oneline -10
git diff HEAD..upstream/master --stat

# QtRest
cd src/external_libs/qtrest_lib/qtrest
git log HEAD..upstream/master --oneline -10
git diff HEAD..upstream/master --stat
```

### 3. Choose update strategy

#### A. Full merge (QtRest)
```bash
./tools/build_helpers/manage_upstream.sh merge qtrest
```

#### B. Cherry-pick (NodeEditor)
```bash
./tools/build_helpers/manage_upstream.sh cherry-pick <commit-hash>
```

#### C. Integration branch for larger updates
```bash
cd src/external_libs/nodeeditor
git checkout -b integration-upstream-$(date +%Y%m%d)
git merge upstream/master
```

## Key Considerations

### NodeEditor specifics
- Contains Daqster-specific behavior on top of upstream.
- Prefer targeted cherry-picks for critical fixes.
- Avoid large blind version jumps.

### QtRest specifics
- Fork is closer to upstream and can be merged more frequently.
- Always re-check Qt5/Qt6 compatibility after updates.

## Troubleshooting

### Merge conflicts
```bash
git status
git diff
git add .
git commit
```

### Cherry-pick conflicts
```bash
git cherry-pick --abort
# or
git add .
git cherry-pick --continue
```

### Missing upstream remote
```bash
git remote -v
git remote add upstream <upstream-url>
```

## Best Practices

1. Run upstream checks at least monthly.
2. Test after every merge/cherry-pick.
3. Keep a short changelog of accepted upstream commits.
4. Prioritize security and crash fixes first.

## Useful Links

- [Git Cherry-pick Guide](https://git-scm.com/docs/git-cherry-pick)
- [Git Merge Strategies](https://git-scm.com/docs/merge-strategies)
- [GitHub Fork Management](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/working-with-forks)
- [NodeEditor Repository](https://github.com/paceholder/nodeeditor)
- [QtRest Repository](https://github.com/kafeg/qtrest)
