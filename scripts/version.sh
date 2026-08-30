#!/usr/bin/env bash
#
# version.sh — get / set / check the project version.
#
# Single source of truth: the root VERSION file.
#
#     ./scripts/version.sh get              # print the current version
#     ./scripts/version.sh set 0.3.0        # bump every location that carries it
#     ./scripts/version.sh check            # verify all locations match VERSION (exit 1 on drift)
#
# The set subcommand is idempotent: locations already at the target version are
# left untouched and reported as "no change".

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

# ── get ──────────────────────────────────────────────────────────────────────
cmd_get() {
    if [ ! -f VERSION ]; then
        echo "ERROR: VERSION file not found" >&2
        exit 1
    fi
    tr -d '[:space:]' < VERSION
}

# ── helpers shared by set/check ──────────────────────────────────────────────
# read_version: prints the current version from VERSION (or exits)
read_version() {
    if [ ! -f VERSION ]; then
        echo "ERROR: VERSION file not found" >&2
        exit 1
    fi
    tr -d '[:space:]' < VERSION
}

# ── set ──────────────────────────────────────────────────────────────────────
cmd_set() {
    if [ "$#" -ne 1 ]; then
        echo "Usage: $0 set <new-version>" >&2
        echo "Example: $0 set 0.3.0" >&2
        exit 1
    fi

    NEW_VERSION="$1"

    if ! printf '%s' "$NEW_VERSION" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
        echo "ERROR: version must be in X.Y.Z format (got '$NEW_VERSION')" >&2
        exit 1
    fi

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

    # NOTE: root CMakeLists.txt is intentionally NOT updated here — it reads the
    # VERSION file at configure time (single source of truth).

    # Plugin CMakeLists project() declarations (3)
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

    # Plugin Interface.json metadata (8)
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

    update_version "src/plugins/QtCoinTrader/QtCoinTraderInterface.json" \
        's/.*"Version": "([0-9.]+)".*/\1/p' \
        "s/(\"Version\": \")[0-9.]+(\")/\1$NEW_VERSION\2/" \
        "QtCoinTraderInterface.json"

    update_version "src/plugins/tests/plugin_main_test/PluginMainTest.json" \
        's/.*"Version": "([0-9.]+)".*/\1/p' \
        "s/(\"Version\": \")[0-9.]+(\")/\1$NEW_VERSION\2/" \
        "PluginMainTest.json"

    update_version "src/plugins/tests/plugin_fancy_test/PluginFancyTest.json" \
        's/.*"Version": "([0-9.]+)".*/\1/p' \
        "s/(\"Version\": \")[0-9.]+(\")/\1$NEW_VERSION\2/" \
        "PluginFancyTest.json"

    update_version "src/plugins/tests/plugin_uggly_test/UgglyTestPlugin.json" \
        's/.*"Version": "([0-9.]+)".*/\1/p' \
        "s/(\"Version\": \")[0-9.]+(\")/\1$NEW_VERSION\2/" \
        "UgglyTestPlugin.json"

    update_version "src/plugins/tests/template_plugin_daqster/DaqsterTeplateInterface.json" \
        's/.*"Version": "([0-9.]+)".*/\1/p' \
        "s/(\"Version\": \")[0-9.]+(\")/\1$NEW_VERSION\2/" \
        "DaqsterTeplateInterface.json"

    # NOTE: *Interface.cpp files are intentionally NOT updated here — they use
    # the DAQSTER_PLUGIN_VERSION compile definition from create_plugin().

    # Plugin docs READMEs (2)
    update_version "docs/plugins/demo_nodeditor_nodes/README.md" \
        's/.*PLUGIN_VERSION    = "([0-9.]+)".*/\1/p' \
        "s/(PLUGIN_VERSION    = \")[0-9.]+(\")/\1$NEW_VERSION\2/" \
        "docs/plugins/demo_nodeditor_nodes/README.md"

    update_version "docs/plugins/node_editor_ide/README.md" \
        's/.*PLUGIN_VERSION    = "([0-9.]+)".*/\1/p' \
        "s/(PLUGIN_VERSION    = \")[0-9.]+(\")/\1$NEW_VERSION\2/" \
        "docs/plugins/node_editor_ide/README.md"

    # Changelogs (2)
    update_changelog "CHANGELOG.md"
    update_changelog "CHANGELOG.en.md"

    echo "----------------------------------------"
    if [ "$CHANGED" -eq 0 ]; then
        echo "No changes — every location is already at $NEW_VERSION."
    else
        echo "Done: $CHANGED location(s) updated to $NEW_VERSION."
    fi
}

# ── check ────────────────────────────────────────────────────────────────────
cmd_check() {
    local VERSION
    VERSION="$(read_version)"

    if ! printf '%s' "$VERSION" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
        echo "FAIL  VERSION format: '$VERSION' is not X.Y.Z" >&2
        exit 1
    fi
    echo "OK    VERSION: $VERSION"

    local FAIL=0

    check_absent() {
        local file="$1" pattern="$2" desc="$3"
        if [ ! -f "$file" ]; then
            echo "MISSING: $file not found"
            FAIL=1
            return
        fi
        if grep -qE "$pattern" "$file"; then
            echo "MISMATCH: $desc — hardcoded version pattern found in $file"
            FAIL=1
        else
            echo "OK    $desc ($file)"
        fi
    }

    check_present() {
        local file="$1" pattern="$2" desc="$3"
        if [ ! -f "$file" ]; then
            echo "MISSING: $file not found"
            FAIL=1
            return
        fi
        if ! grep -qE "$pattern" "$file"; then
            echo "MISMATCH: $desc — $file does not contain version $VERSION"
            FAIL=1
        else
            echo "OK    $desc ($file)"
        fi
    }

    # Root CMakeLists must NOT hardcode project(Daqster VERSION X.Y.Z) — it reads VERSION
    check_absent "CMakeLists.txt" \
        "project\(Daqster VERSION [0-9]" \
        "root CMakeLists.txt reads VERSION (no hardcoded project version)"

    # Plugin CMakeLists project() declarations (3)
    check_present "src/plugins/demo_nodeditor_nodes/CMakeLists.txt" \
        "project\(demo_nodeditor_nodes VERSION $VERSION" \
        "demo_nodeditor_nodes CMakeLists.txt"
    check_present "src/plugins/node_editor_ide/CMakeLists.txt" \
        "project\(node_editor_ide VERSION $VERSION" \
        "node_editor_ide CMakeLists.txt"
    check_present "src/plugins/requirements_manager/CMakeLists.txt" \
        "project\(requirements_manager VERSION $VERSION" \
        "requirements_manager CMakeLists.txt"

    # Plugin Interface.json metadata (8)
    check_present "src/plugins/demo_nodeditor_nodes/DemoNodeEditorNodesInterface.json" \
        "\"Version\": \"$VERSION\"" \
        "DemoNodeEditorNodesInterface.json"
    check_present "src/plugins/node_editor_ide/NodeEditorIdeInterface.json" \
        "\"Version\": \"$VERSION\"" \
        "NodeEditorIdeInterface.json"
    check_present "src/plugins/requirements_manager/RequirementsManagerInterface.json" \
        "\"Version\": \"$VERSION\"" \
        "RequirementsManagerInterface.json"
    check_present "src/plugins/QtCoinTrader/QtCoinTraderInterface.json" \
        "\"Version\": \"$VERSION\"" \
        "QtCoinTraderInterface.json"
    check_present "src/plugins/tests/plugin_main_test/PluginMainTest.json" \
        "\"Version\": \"$VERSION\"" \
        "PluginMainTest.json"
    check_present "src/plugins/tests/plugin_fancy_test/PluginFancyTest.json" \
        "\"Version\": \"$VERSION\"" \
        "PluginFancyTest.json"
    check_present "src/plugins/tests/plugin_uggly_test/UgglyTestPlugin.json" \
        "\"Version\": \"$VERSION\"" \
        "UgglyTestPlugin.json"
    check_present "src/plugins/tests/template_plugin_daqster/DaqsterTeplateInterface.json" \
        "\"Version\": \"$VERSION\"" \
        "DaqsterTeplateInterface.json"

    # No hardcoded PLUGIN_VERSION literal in any *Interface.cpp (must use the macro)
    local cpp_hits
    cpp_hits="$(grep -rEn 'PLUGIN_VERSION, "' src/plugins --include='*Interface.cpp' || true)"
    if [ -n "$cpp_hits" ]; then
        echo "MISMATCH: hardcoded PLUGIN_VERSION literal found in *Interface.cpp:"
        echo "$cpp_hits"
        FAIL=1
    else
        echo "OK    *Interface.cpp use DAQSTER_PLUGIN_VERSION (no hardcoded literals)"
    fi

    # Plugin docs READMEs (2)
    check_present "docs/plugins/demo_nodeditor_nodes/README.md" \
        "PLUGIN_VERSION    = \"$VERSION\"" \
        "docs/plugins/demo_nodeditor_nodes/README.md"
    check_present "docs/plugins/node_editor_ide/README.md" \
        "PLUGIN_VERSION    = \"$VERSION\"" \
        "docs/plugins/node_editor_ide/README.md"

    # Changelogs (2)
    check_present "CHANGELOG.md" "^## \[$VERSION\]" "CHANGELOG.md"
    check_present "CHANGELOG.en.md" "^## \[$VERSION\]" "CHANGELOG.en.md"

    # main.cpp uses the central version macro (no "0.1" literal)
    check_present "src/apps/Daqster/main.cpp" \
        "DAQSTER_VERSION_STRING" \
        "main.cpp uses DAQSTER_VERSION_STRING"
    check_absent "src/apps/Daqster/main.cpp" \
        "setApplicationVersion\(\"0\.1\"\)" \
        "main.cpp has no hardcoded \"0.1\" version"

    if [ "$FAIL" -ne 0 ]; then
        echo ""
        echo "Version drift detected. Run ./scripts/version.sh set $VERSION to sync all locations."
        exit 1
    fi
    echo ""
    echo "All version locations match VERSION ($VERSION)."
}

# ── dispatch ─────────────────────────────────────────────────────────────────
case "${1:-}" in
    get)
        cmd_get
        ;;
    set)
        shift
        cmd_set "$@"
        ;;
    check)
        cmd_check
        ;;
    *)
        echo "Usage: $0 {get|set <X.Y.Z>|check}" >&2
        echo "  get    — print the current version from VERSION" >&2
        echo "  set    — bump every location to <X.Y.Z>" >&2
        echo "  check  — verify all locations match VERSION (exit 1 on drift)" >&2
        exit 1
        ;;
esac