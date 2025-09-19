#!/bin/bash

# Upstream Management Script for Daqster External Libraries
# This script helps manage upstream tracking for NodeEditor and QtRest libraries

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if we're in the right directory
check_directory() {
    if [ ! -f "CMakeLists.txt" ] || [ ! -d "src/external_libs" ]; then
        print_error "This script must be run from the Daqster project root directory"
        exit 1
    fi
}

# Function to show help
show_help() {
    echo "Upstream Management Script for Daqster External Libraries"
    echo ""
    echo "Usage: $0 [COMMAND] [OPTIONS]"
    echo ""
    echo "Commands:"
    echo "  status          - Show status of all external libraries"
    echo "  fetch           - Fetch latest changes from upstream"
    echo "  check           - Check for new commits in upstream"
    echo "  merge [LIB]     - Merge upstream changes (LIB: nodeeditor|qtrest|all)"
    echo "  cherry-pick     - Cherry-pick specific commits from upstream"
    echo "  setup           - Setup upstream tracking for all libraries"
    echo "  help            - Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 status                    # Show current status"
    echo "  $0 fetch                     # Fetch all upstream changes"
    echo "  $0 check                     # Check for new commits"
    echo "  $0 merge nodeeditor          # Merge NodeEditor upstream changes"
    echo "  $0 merge all                 # Merge all upstream changes"
    echo "  $0 cherry-pick <commit-hash> # Cherry-pick specific commit"
}

# Function to setup upstream tracking
setup_upstream() {
    print_status "Setting up upstream tracking for external libraries..."
    
    # NodeEditor
    if [ -d "src/external_libs/nodeeditor" ]; then
        print_status "Setting up NodeEditor upstream tracking..."
        cd src/external_libs/nodeeditor
        if ! git remote | grep -q upstream; then
            git remote add upstream https://github.com/paceholder/nodeeditor.git
            print_success "Added upstream remote for NodeEditor"
        else
            print_warning "Upstream remote already exists for NodeEditor"
        fi
        cd ../../..
    fi
    
    # QtRest
    if [ -d "src/external_libs/qtrest_lib/qtrest" ]; then
        print_status "Setting up QtRest upstream tracking..."
        cd src/external_libs/qtrest_lib/qtrest
        if ! git remote | grep -q upstream; then
            git remote add upstream https://github.com/kafeg/qtrest.git
            print_success "Added upstream remote for QtRest"
        else
            print_warning "Upstream remote already exists for QtRest"
        fi
        cd ../../..
    fi
    
    print_success "Upstream tracking setup completed"
}

# Function to show status
show_status() {
    print_status "External Libraries Status:"
    echo ""
    
    # NodeEditor
    if [ -d "src/external_libs/nodeeditor" ]; then
        print_status "=== NodeEditor ==="
        cd src/external_libs/nodeeditor
        echo "Current branch: $(git branch --show-current)"
        echo "Current commit: $(git rev-parse --short HEAD)"
        echo "Upstream status:"
        if git remote | grep -q upstream; then
            git fetch upstream -q
            BEHIND=$(git rev-list --count HEAD..upstream/master 2>/dev/null || echo "0")
            AHEAD=$(git rev-list --count upstream/master..HEAD 2>/dev/null || echo "0")
            echo "  - Behind upstream: $BEHIND commits"
            echo "  - Ahead of upstream: $AHEAD commits"
        else
            echo "  - No upstream remote configured"
        fi
        cd ../../..
        echo ""
    fi
    
    # QtRest
    if [ -d "src/external_libs/qtrest_lib/qtrest" ]; then
        print_status "=== QtRest ==="
        cd src/external_libs/qtrest_lib/qtrest
        echo "Current branch: $(git branch --show-current)"
        echo "Current commit: $(git rev-parse --short HEAD)"
        echo "Upstream status:"
        if git remote | grep -q upstream; then
            git fetch upstream -q
            BEHIND=$(git rev-list --count HEAD..upstream/master 2>/dev/null || echo "0")
            AHEAD=$(git rev-list --count upstream/master..HEAD 2>/dev/null || echo "0")
            echo "  - Behind upstream: $BEHIND commits"
            echo "  - Ahead of upstream: $AHEAD commits"
        else
            echo "  - No upstream remote configured"
        fi
        cd ../../..
        echo ""
    fi
}

# Function to fetch upstream changes
fetch_upstream() {
    print_status "Fetching upstream changes..."
    
    # NodeEditor
    if [ -d "src/external_libs/nodeeditor" ] && git -C src/external_libs/nodeeditor remote | grep -q upstream; then
        print_status "Fetching NodeEditor upstream..."
        git -C src/external_libs/nodeeditor fetch upstream
        print_success "NodeEditor upstream fetched"
    fi
    
    # QtRest
    if [ -d "src/external_libs/qtrest_lib/qtrest" ] && git -C src/external_libs/qtrest_lib/qtrest remote | grep -q upstream; then
        print_status "Fetching QtRest upstream..."
        git -C src/external_libs/qtrest_lib/qtrest fetch upstream
        print_success "QtRest upstream fetched"
    fi
    
    print_success "All upstream changes fetched"
}

# Function to check for new commits
check_upstream() {
    print_status "Checking for new upstream commits..."
    echo ""
    
    # NodeEditor
    if [ -d "src/external_libs/nodeeditor" ] && git -C src/external_libs/nodeeditor remote | grep -q upstream; then
        print_status "=== NodeEditor ==="
        cd src/external_libs/nodeeditor
        git fetch upstream -q
        NEW_COMMITS=$(git rev-list --count HEAD..upstream/master 2>/dev/null || echo "0")
        if [ "$NEW_COMMITS" -gt 0 ]; then
            print_warning "Found $NEW_COMMITS new commits in NodeEditor upstream"
            echo "Recent commits:"
            git log HEAD..upstream/master --oneline -5
        else
            print_success "NodeEditor is up to date with upstream"
        fi
        cd ../../..
        echo ""
    fi
    
    # QtRest
    if [ -d "src/external_libs/qtrest_lib/qtrest" ] && git -C src/external_libs/qtrest_lib/qtrest remote | grep -q upstream; then
        print_status "=== QtRest ==="
        cd src/external_libs/qtrest_lib/qtrest
        git fetch upstream -q
        NEW_COMMITS=$(git rev-list --count HEAD..upstream/master 2>/dev/null || echo "0")
        if [ "$NEW_COMMITS" -gt 0 ]; then
            print_warning "Found $NEW_COMMITS new commits in QtRest upstream"
            echo "Recent commits:"
            git log HEAD..upstream/master --oneline -5
        else
            print_success "QtRest is up to date with upstream"
        fi
        cd ../../..
        echo ""
    fi
}

# Function to merge upstream changes
merge_upstream() {
    local lib="$1"
    
    if [ -z "$lib" ]; then
        print_error "Please specify library: nodeeditor, qtrest, or all"
        exit 1
    fi
    
    case "$lib" in
        "nodeeditor")
            if [ -d "src/external_libs/nodeeditor" ] && git -C src/external_libs/nodeeditor remote | grep -q upstream; then
                print_status "Merging NodeEditor upstream changes..."
                cd src/external_libs/nodeeditor
                git fetch upstream
                git merge upstream/master
                print_success "NodeEditor upstream changes merged"
                cd ../../..
            else
                print_error "NodeEditor upstream not configured"
            fi
            ;;
        "qtrest")
            if [ -d "src/external_libs/qtrest_lib/qtrest" ] && git -C src/external_libs/qtrest_lib/qtrest remote | grep -q upstream; then
                print_status "Merging QtRest upstream changes..."
                cd src/external_libs/qtrest_lib/qtrest
                git fetch upstream
                git merge upstream/master
                print_success "QtRest upstream changes merged"
                cd ../../..
            else
                print_error "QtRest upstream not configured"
            fi
            ;;
        "all")
            merge_upstream "nodeeditor"
            merge_upstream "qtrest"
            ;;
        *)
            print_error "Unknown library: $lib. Use: nodeeditor, qtrest, or all"
            exit 1
            ;;
    esac
}

# Function to cherry-pick commits
cherry_pick_commit() {
    local commit_hash="$1"
    
    if [ -z "$commit_hash" ]; then
        print_error "Please provide commit hash to cherry-pick"
        exit 1
    fi
    
    print_status "Cherry-picking commit: $commit_hash"
    print_warning "This will attempt to cherry-pick from both libraries"
    print_warning "You may need to specify which library manually"
    
    # Try NodeEditor first
    if [ -d "src/external_libs/nodeeditor" ]; then
        print_status "Trying NodeEditor..."
        cd src/external_libs/nodeeditor
        if git show "$commit_hash" >/dev/null 2>&1; then
            git cherry-pick "$commit_hash"
            print_success "Cherry-picked $commit_hash from NodeEditor"
            cd ../../..
            return 0
        fi
        cd ../../..
    fi
    
    # Try QtRest
    if [ -d "src/external_libs/qtrest_lib/qtrest" ]; then
        print_status "Trying QtRest..."
        cd src/external_libs/qtrest_lib/qtrest
        if git show "$commit_hash" >/dev/null 2>&1; then
            git cherry-pick "$commit_hash"
            print_success "Cherry-picked $commit_hash from QtRest"
            cd ../../..
            return 0
        fi
        cd ../../..
    fi
    
    print_error "Commit $commit_hash not found in any library"
}

# Main script logic
main() {
    check_directory
    
    case "${1:-help}" in
        "setup")
            setup_upstream
            ;;
        "status")
            show_status
            ;;
        "fetch")
            fetch_upstream
            ;;
        "check")
            check_upstream
            ;;
        "merge")
            merge_upstream "$2"
            ;;
        "cherry-pick")
            cherry_pick_commit "$2"
            ;;
        "help"|*)
            show_help
            ;;
    esac
}

# Run main function with all arguments
main "$@"
