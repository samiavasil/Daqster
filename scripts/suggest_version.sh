#!/usr/bin/env bash
#
# suggest_version.sh — release-time semver bump check.
#
# Reads the current version from the root VERSION file, inspects the git history
# since the last tag (v*), classifies the commits (feat → minor, fix → patch,
# breaking → major) and proposes the next semver version with a rationale.
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

# Last tag (v*), or treat as v0.0.0 when no tag exists yet
LAST_TAG="$(git describe --tags --abbrev=0 --match 'v*' 2>/dev/null || true)"
if [ -z "$LAST_TAG" ]; then
    LAST_TAG="v0.0.0"
    echo "No previous v* tag found — treating baseline as v0.0.0"
fi

echo "Current version : $CURRENT"
echo "Last tag       : $LAST_TAG"
echo "----------------------------------------"

# ── Classify commits since the last tag ──────────────────────────────────────
# Breaking paths: the plugin IID version macros and the shared plugin API
# (NodeDataTypes / capabilities). A commit touching any of these, or whose
# message contains BREAKING / BREAKING CHANGE, forces a MAJOR bump.
BREAKING_PATHS=(
    "src/frame_work/base/src/include/QPluginInterface.h"
    "src/plugins/common/"
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
    PROPOSED="$((MAJOR + 1)).0.0"
    REASON="breaking change(s) detected (plugin API / shared types / BREAKING marker)"
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