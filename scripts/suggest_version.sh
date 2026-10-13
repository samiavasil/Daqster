#!/usr/bin/env bash
#
# suggest_version.sh — release-time semver bump check.
#
# Reads the current version from the root VERSION file, inspects the git history
# since the last RELEASE tag (the v* tag matching VERSION — see
# RELEASE-PROCESS.md §1), classifies the commits (feat → minor, fix → patch,
# breaking → major) and proposes the next semver version with a rationale.
# At 0.x, breaking changes fold into a MINOR bump (MAJOR stays 0) per
# RELEASE-PROCESS.md §1.
#
# Usage:
#     ./scripts/suggest_version.sh            # interactive (asks before applying)
#     ./scripts/suggest_version.sh --yes      # apply the proposal without asking
#     ./scripts/suggest_version.sh --dry-run  # print the proposal only
#
# On "yes" the script runs `./scripts/version.sh set <proposed>`.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

YES=0
DRY_RUN=0
for arg in "$@"; do
    case "$arg" in
        --yes) YES=1 ;;
        --dry-run) DRY_RUN=1 ;;
        *)
            echo "Usage: $0 [--yes] [--dry-run]" >&2
            exit 1
            ;;
    esac
done

if [ ! -f VERSION ]; then
    echo "ERROR: VERSION file not found" >&2
    exit 1
fi

CURRENT="$(tr -d '[:space:]' < VERSION)"
if ! printf '%s' "$CURRENT" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "ERROR: VERSION must be X.Y.Z (got '$CURRENT')" >&2
    exit 1
fi

# Baseline tag = the last RELEASE (v*). Prefer the tag that matches VERSION
# (the documented single source of truth, see RELEASE-PROCESS.md §1). Do NOT
# use `git describe`: it is ancestry-based, and the release tags live on
# `master`, which is not an ancestor of `develop` — describe would then return
# an ancient tag (e.g. v0.0.1-beta) and classify hundreds of unrelated commits.
LAST_TAG="v${CURRENT}"
if ! git rev-parse -q --verify "refs/tags/${LAST_TAG}" >/dev/null 2>&1; then
    LAST_TAG="$(git tag --list 'v*' --sort=-v:refname 2>/dev/null | head -n1 || true)"
fi
if [ -z "$LAST_TAG" ]; then
    LAST_TAG="v0.0.0"
    echo "No previous v* tag found — treating baseline as v0.0.0"
fi

echo "Current version : $CURRENT"
echo "Last tag       : $LAST_TAG"
echo "----------------------------------------"

# ── Classify commits since the last tag ──────────────────────────────────────
# Breaking paths: the host↔plugin ABI surface — the plugin IID/interface header
# and the shared plugin API (capabilities + NodeDataTypes + export macros). A
# commit touching any of these, or whose message contains BREAKING / BREAKING
# CHANGE, is treated as a potential break. NOTE: this is a heuristic — it cannot
# tell additive from incompatible changes; keeping it narrow avoids flagging
# every commit that merely lives under src/plugins/common/.
BREAKING_PATHS=(
    "src/frame_work/base/src/include/QPluginInterface.h"
    "src/plugins/common/capabilities/"
    "src/plugins/common/NodeDataTypes/"
    "src/plugins/common/DaqsterCommonExport.h"
)

HAS_BREAKING=0
HAS_FEAT=0
HAS_FIX=0
COMMIT_COUNT=0

while IFS= read -r line; do
    [ -z "$line" ] && continue
    COMMIT_COUNT=$((COMMIT_COUNT + 1))
    hash="${line%% *}"
    subject="${line#* }"

    # Breaking: message marker
    if printf '%s' "$subject" | grep -qiE 'BREAKING( CHANGE)?'; then
        echo "BREAKING commit $hash: $subject"
        HAS_BREAKING=1
        continue
    fi

    # Breaking: touched paths
    for path in "${BREAKING_PATHS[@]}"; do
        if git diff-tree --no-commit-id --name-only -r "$hash" 2>/dev/null | grep -q "^${path}"; then
            echo "BREAKING commit $hash (touches $path): $subject"
            HAS_BREAKING=1
            break
        fi
    done
    [ "$HAS_BREAKING" -eq 1 ] && continue

    # Conventional commit type (AGENTS.md: "<type> #<issue> (REQ-...): <desc>")
    case "$subject" in
        feat*)
            echo "FEAT    commit $hash: $subject"
            HAS_FEAT=1
            ;;
        fix*)
            echo "FIX     commit $hash: $subject"
            HAS_FIX=1
            ;;
        *)
            echo "other   commit $hash: $subject"
            ;;
    esac
done < <(git log --oneline "${LAST_TAG}..HEAD" 2>/dev/null || true)

if [ "$COMMIT_COUNT" -eq 0 ]; then
    echo ""
    echo "No commits since $LAST_TAG — no bump needed."
    exit 0
fi

# ── Propose the next version ─────────────────────────────────────────────────
IFS='.' read -r MAJOR MINOR PATCH <<< "$CURRENT"

if [ "$HAS_BREAKING" -eq 1 ]; then
    if [ "$MAJOR" -eq 0 ]; then
        # RELEASE-PROCESS.md §1: while 0.x, breaking changes fold into a MINOR
        # bump; MAJOR stays 0 until the project is declared stable.
        PROPOSED="0.$((MINOR + 1)).0"
        REASON="breaking change(s) at 0.x — MINOR bump (RELEASE-PROCESS.md §1)"
    else
        PROPOSED="$((MAJOR + 1)).0.0"
        REASON="breaking change(s) detected (plugin API / shared types / BREAKING marker)"
    fi
elif [ "$HAS_FEAT" -eq 1 ]; then
    PROPOSED="$MAJOR.$((MINOR + 1)).0"
    REASON="new feature(s) detected (feat commits)"
elif [ "$HAS_FIX" -eq 1 ]; then
    PROPOSED="$MAJOR.$MINOR.$((PATCH + 1))"
    REASON="bugfix(es) detected (fix commits)"
else
    echo ""
    echo "No feat/fix/breaking commits since $LAST_TAG — no bump needed."
    exit 0
fi

echo ""
echo "Proposed version: $PROPOSED"
echo "Rationale       : $REASON"
echo "----------------------------------------"

if [ "$DRY_RUN" -eq 1 ]; then
    echo "Dry run — not applying. Run without --dry-run to apply."
    exit 0
fi

if [ "$YES" -eq 1 ]; then
    APPLY="y"
else
    read -r -p "Apply? [y/N] " APPLY
fi

if [ "$APPLY" = "y" ] || [ "$APPLY" = "Y" ]; then
    echo "Applying: ./scripts/version.sh set $PROPOSED"
    ./scripts/version.sh set "$PROPOSED"
else
    echo "Not applying."
    exit 0
fi