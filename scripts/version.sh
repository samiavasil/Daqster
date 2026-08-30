#!/usr/bin/env bash
#
# version.sh — bump the project version in every location that carries it.
#
# Single source of truth: the root VERSION file. Run this script after updating
# VERSION, or pass the new version directly:
#
#     ./scripts/version.sh 0.3.0
#
# The script is idempotent: locations already at the target version are left
# untouched and reported as "no change".

set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <new-version>" >&2
    echo "Example: $0 0.3.0" >&2
    exit 1
fi

NEW_VERSION="$1"

if ! printf '%s' "$NEW_VERSION" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "ERROR: version must be in X.Y.Z format (got '$NEW_VERSION')" >&2
    exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

CHANGED=0

# update_version <file> <extract-sed> <update-sed> <description>
#   extract-sed: prints the current version from the file (first match)
#   update-sed:  rewrites the version in place
update_version() {
    local file="$1" extract_sed="$2" update_sed="$3" desc="$4"
    if [ ! -f "$file" ]; then
        echo "SKIP  $desc: $file not found"
        return
    fi
    local current
    current="$(sed -nE "$extract_sed" "$file" | head -n1 || true)"
    if [ -z "$current" ]; then
        echo "SKIP  $desc: no version pattern found in $file"
        return
    fi
    if [ "$current" = "$NEW_VERSION" ]; then
        echo "OK    $desc: already $NEW_VERSION ($file)"
        return
    fi
    sed -i -E "$update_sed" "$file"
    echo "UPDATED $desc: $current -> $NEW_VERSION ($file)"
    CHANGED=$((CHANGED + 1))
}

# update_changelog <file>
#   Adds a "## [X.Y.Z] - YYYY-MM-DD" section below "## [Unreleased]" when the
#   section does not already exist (Keep a Changelog release convention: the
#   unreleased entries become the new release, a fresh [Unreleased] stays on top).
update_changelog() {
    local file="$1"
    if [ ! -f "$file" ]; then
        echo "SKIP  changelog: $file not found"
        return
    fi
    if grep -q "^## \[$NEW_VERSION\]" "$file"; then
        echo "OK    changelog: section [$NEW_VERSION] already present ($file)"
        return
    fi
    local date
    date="$(date +%Y-%m-%d)"
    sed -i "0,/^## \[Unreleased\]/s//## [Unreleased]\n\n## [$NEW_VERSION] - $date/" "$file"
    echo "UPDATED changelog: added [$NEW_VERSION] - $date section ($file)"
    CHANGED=$((CHANGED + 1))
}

echo "Bumping version to $NEW_VERSION"
echo "----------------------------------------"

# Root VERSION file (single source of truth)
if [ -f VERSION ]; then
    current="$(tr -d '[:space:]' < VERSION)"
    if [ "$current" = "$NEW_VERSION" ]; then
        echo "OK    VERSION: already $NEW_VERSION"
    else
        printf '%s\n' "$NEW_VERSION" > VERSION
        echo "UPDATED VERSION: $current -> $NEW_VERSION"
        CHANGED=$((CHANGED + 1))
    fi
else
    echo "SKIP  VERSION: file not found"
fi

# CMake project() declarations
update_version "CMakeLists.txt" \
    's/.*project\([a-zA-Z_]+ VERSION ([0-9.]+).*/\1/p' \
    "s/(project\([a-zA-Z_]+ VERSION )[0-9.]+/\1$NEW_VERSION/" \
    "root CMakeLists.txt"

update_version "src/plugins/demo_nodeditor_nodes/CMakeLists.txt" \
    's/.*project\([a-zA-Z_]+ VERSION ([0-9.]+).*/\1/p' \
    "s/(project\([a-zA-Z_]+ VERSION )[0-9.]+/\1$NEW_VERSION/" \
    "demo_nodeditor_nodes CMakeLists.txt"

update_version "src/plugins/node_editor_ide/CMakeLists.txt" \
    's/.*project\([a-zA-Z_]+ VERSION ([0-9.]+).*/\1/p' \
    "s/(project\([a-zA-Z_]+ VERSION )[0-9.]+/\1$NEW_VERSION/" \
    "node_editor_ide CMakeLists.txt"

update_version "src/plugins/requirements_manager/CMakeLists.txt" \
    's/.*project\([a-zA-Z_]+ VERSION ([0-9.]+).*/\1/p' \
    "s/(project\([a-zA-Z_]+ VERSION )[0-9.]+/\1$NEW_VERSION/" \
    "requirements_manager CMakeLists.txt"

# Plugin Interface.json metadata
update_version "src/plugins/demo_nodeditor_nodes/DemoNodeEditorNodesInterface.json" \
    's/.*"Version": "([0-9.]+)".*/\1/p' \
    "s/(\"Version\": \")[0-9.]+(\")/\1$NEW_VERSION\2/" \
    "DemoNodeEditorNodesInterface.json"

update_version "src/plugins/node_editor_ide/NodeEditorIdeInterface.json" \
    's/.*"Version": "([0-9.]+)".*/\1/p' \
    "s/(\"Version\": \")[0-9.]+(\")/\1$NEW_VERSION\2/" \
    "NodeEditorIdeInterface.json"

update_version "src/plugins/requirements_manager/RequirementsManagerInterface.json" \
    's/.*"Version": "([0-9.]+)".*/\1/p' \
    "s/(\"Version\": \")[0-9.]+(\")/\1$NEW_VERSION\2/" \
    "RequirementsManagerInterface.json"

# Plugin Interface.cpp PLUGIN_VERSION
update_version "src/plugins/demo_nodeditor_nodes/DemoNodeEditorNodesInterface.cpp" \
    's/.*PLUGIN_VERSION, "([0-9.]+)".*/\1/p' \
    "s/(PLUGIN_VERSION, \")[0-9.]+(\")/\1$NEW_VERSION\2/" \
    "DemoNodeEditorNodesInterface.cpp"

update_version "src/plugins/node_editor_ide/NodeEditorIdeInterface.cpp" \
    's/.*PLUGIN_VERSION, "([0-9.]+)".*/\1/p' \
    "s/(PLUGIN_VERSION, \")[0-9.]+(\")/\1$NEW_VERSION\2/" \
    "NodeEditorIdeInterface.cpp"

update_version "src/plugins/requirements_manager/RequirementsManagerInterface.cpp" \
    's/.*PLUGIN_VERSION, "([0-9.]+)".*/\1/p' \
    "s/(PLUGIN_VERSION, \")[0-9.]+(\")/\1$NEW_VERSION\2/" \
    "RequirementsManagerInterface.cpp"

# Plugin docs READMEs
update_version "docs/plugins/demo_nodeditor_nodes/README.md" \
    's/.*PLUGIN_VERSION    = "([0-9.]+)".*/\1/p' \
    "s/(PLUGIN_VERSION    = \")[0-9.]+(\")/\1$NEW_VERSION\2/" \
    "docs/plugins/demo_nodeditor_nodes/README.md"

update_version "docs/plugins/node_editor_ide/README.md" \
    's/.*PLUGIN_VERSION    = "([0-9.]+)".*/\1/p' \
    "s/(PLUGIN_VERSION    = \")[0-9.]+(\")/\1$NEW_VERSION\2/" \
    "docs/plugins/node_editor_ide/README.md"

# Changelogs
update_changelog "CHANGELOG.md"
update_changelog "CHANGELOG.en.md"

echo "----------------------------------------"
if [ "$CHANGED" -eq 0 ]; then
    echo "No changes — every location is already at $NEW_VERSION."
else
    echo "Done: $CHANGED location(s) updated to $NEW_VERSION."
fi