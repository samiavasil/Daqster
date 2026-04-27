#!/bin/bash

# Upstream Management Script for Daqster External Libraries (moved)
# Improved: configurable upstream URLs, dry-run for merge/cherry-pick, confirmation prompt

set -euo pipefail

DEFAULT_NODEEDITOR_UPSTREAM="https://github.com/paceholder/nodeeditor.git"
DEFAULT_QTREST_UPSTREAM="https://github.com/kafeg/qtrest.git"

# Allow overriding via env or CLI
NODEEDITOR_UPSTREAM=${NODEEDITOR_UPSTREAM:-$DEFAULT_NODEEDITOR_UPSTREAM}
QTREST_UPSTREAM=${QTREST_UPSTREAM:-$DEFAULT_QTREST_UPSTREAM}

DRY_RUN=0
AUTO_YES=0

usage() {
    cat <<'EOF'
Usage: manage_upstream.sh [--dry-run] [--yes] <command> [args]

Commands: status | fetch | check | merge <lib|all> | cherry-pick <commit> | setup | help

Environment:
  NODEEDITOR_UPSTREAM   override NodeEditor upstream URL
  QTREST_UPSTREAM       override QtRest upstream URL
EOF
}

confirm() {
    if [ "$AUTO_YES" -eq 1 ]; then
        return 0
    fi
    read -r -p "Proceed? [y/N]: " ans
    case "$ans" in
        [Yy]*) return 0;;
        *) return 1;;
    esac
}

print() { echo "[INFO] $*"; }
warn() { echo "[WARN] $*" >&2; }
err() { echo "[ERROR] $*" >&2; }

check_directory() {
    if [ ! -f "CMakeLists.txt" ] || [ ! -d "src/external_libs" ]; then
        err "Run from project root"
        exit 1
    fi
}

setup_upstream() {
    print "Setting up upstream tracking..."
    if [ -d "src/external_libs/nodeeditor" ]; then
        print "NodeEditor: setting upstream => ${NODEEDITOR_UPSTREAM}"
        cd src/external_libs/nodeeditor
        if ! git remote | grep -q upstream; then
            git remote add upstream "${NODEEDITOR_UPSTREAM}"
        else
            print "Upstream exists"
        fi
        cd - >/dev/null
    fi
    if [ -d "src/external_libs/qtrest_lib/qtrest" ]; then
        print "QtRest: setting upstream => ${QTREST_UPSTREAM}"
        cd src/external_libs/qtrest_lib/qtrest
        if ! git remote | grep -q upstream; then
            git remote add upstream "${QTREST_UPSTREAM}"
        else
            print "Upstream exists"
        fi
        cd - >/dev/null
    fi
}

show_status() {
    print "External Libraries Status:"; echo
    if [ -d "src/external_libs/nodeeditor" ]; then
        print "=== NodeEditor ==="
        cd src/external_libs/nodeeditor
        echo "Branch: $(git branch --show-current)"
        echo "Commit: $(git rev-parse --short HEAD)"
        if git remote | grep -q upstream; then
            git fetch upstream -q
            BEHIND=$(git rev-list --count HEAD..upstream/master 2>/dev/null || echo "0")
            AHEAD=$(git rev-list --count upstream/master..HEAD 2>/dev/null || echo "0")
            echo "  - Behind upstream: $BEHIND"
            echo "  - Ahead of upstream: $AHEAD"
        else
            echo "  - No upstream remote configured"
        fi
        cd - >/dev/null
        echo
    fi
    if [ -d "src/external_libs/qtrest_lib/qtrest" ]; then
        print "=== QtRest ==="
        cd src/external_libs/qtrest_lib/qtrest
        echo "Branch: $(git branch --show-current)"
        echo "Commit: $(git rev-parse --short HEAD)"
        if git remote | grep -q upstream; then
            git fetch upstream -q
            BEHIND=$(git rev-list --count HEAD..upstream/master 2>/dev/null || echo "0")
            AHEAD=$(git rev-list --count upstream/master..HEAD 2>/dev/null || echo "0")
            echo "  - Behind upstream: $BEHIND"
            echo "  - Ahead of upstream: $AHEAD"
        else
            echo "  - No upstream remote configured"
        fi
        cd - >/dev/null
        echo
    fi
}

fetch_upstream() {
    print "Fetching upstream..."
    if [ -d "src/external_libs/nodeeditor" ] && git -C src/external_libs/nodeeditor remote | grep -q upstream; then
        git -C src/external_libs/nodeeditor fetch upstream
    fi
    if [ -d "src/external_libs/qtrest_lib/qtrest" ] && git -C src/external_libs/qtrest_lib/qtrest remote | grep -q upstream; then
        git -C src/external_libs/qtrest_lib/qtrest fetch upstream
    fi
}

check_upstream() {
    print "Checking for new upstream commits..."
    if [ -d "src/external_libs/nodeeditor" ] && git -C src/external_libs/nodeeditor remote | grep -q upstream; then
        cd src/external_libs/nodeeditor
        git fetch upstream -q
        NEW=$(git rev-list --count HEAD..upstream/master 2>/dev/null || echo "0")
        if [ "$NEW" -gt 0 ]; then
            warn "NodeEditor has $NEW new commits"
            git log HEAD..upstream/master --oneline -5
        else
            print "NodeEditor up to date"
        fi
        cd - >/dev/null
    fi
    if [ -d "src/external_libs/qtrest_lib/qtrest" ] && git -C src/external_libs/qtrest_lib/qtrest remote | grep -q upstream; then
        cd src/external_libs/qtrest_lib/qtrest
        git fetch upstream -q
        NEW=$(git rev-list --count HEAD..upstream/master 2>/dev/null || echo "0")
        if [ "$NEW" -gt 0 ]; then
            warn "QtRest has $NEW new commits"
            git log HEAD..upstream/master --oneline -5
        else
            print "QtRest up to date"
        fi
        cd - >/dev/null
    fi
}

merge_upstream() {
    lib="$1"
    if [ "$DRY_RUN" -eq 1 ]; then
        print "Dry-run: would merge upstream for $lib"
        return 0
    fi
    case "$lib" in
        nodeeditor)
            dir=src/external_libs/nodeeditor
            ;;
        qtrest)
            dir=src/external_libs/qtrest_lib/qtrest
            ;;
        all)
            merge_upstream nodeeditor
            merge_upstream qtrest
            return 0
            ;;
        *) err "Unknown library: $lib"; return 1 ;;
    esac
    if [ ! -d "$dir" ]; then err "$dir not found"; return 1; fi
    if ! git -C "$dir" remote | grep -q upstream; then err "No upstream for $dir"; return 1; fi
    print "About to merge upstream into $dir"
    if confirm; then
        git -C "$dir" fetch upstream
        git -C "$dir" merge upstream/master
    else
        print "Aborted by user"
    fi
}

cherry_pick_commit() {
    commit="$1"
    if [ "$DRY_RUN" -eq 1 ]; then
        print "Dry-run: would cherry-pick $commit"
        return 0
    fi
    print "Attempting to cherry-pick $commit"
    if confirm; then
        for d in src/external_libs/nodeeditor src/external_libs/qtrest_lib/qtrest; do
            if [ -d "$d" ]; then
                if git -C "$d" show "$commit" >/dev/null 2>&1; then
                    git -C "$d" cherry-pick "$commit"
                    print "Cherry-picked in $d"
                    return 0
                fi
            fi
        done
        err "Commit not found"
    else
        print "Aborted by user"
    fi
}

main() {
    check_directory
    # parse global flags
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --dry-run) DRY_RUN=1; shift ;;
            --yes) AUTO_YES=1; shift ;;
            --help|-h) usage; exit 0 ;;
            *) break ;;
        esac
    done

    cmd=${1:-help}
    case "$cmd" in
        setup) setup_upstream ;;
        status) show_status ;;
        fetch) fetch_upstream ;;
        check) check_upstream ;;
        merge) merge_upstream "$2" ;;
        cherry-pick) cherry_pick_commit "$2" ;;
        help|*) usage ;;
    esac
}

main "$@"
