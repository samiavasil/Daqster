#!/bin/bash

# debug_control.sh (moved)
# Adds restore command and safer sudo handling

set -euo pipefail

BACKUP_FILE=/tmp/daqt_ptrace_scope_backup

show_help() {
    cat <<EOF
Usage: debug_control.sh <on|off|status|restore|help> [--yes]

Commands:
  on|enable    - set ptrace_scope=0 (allow attach)
  off|disable  - set ptrace_scope=1 (restrict)
  status       - show current
  restore      - restore previous value (if saved)
  --yes        - don't prompt for confirmation
EOF
}

confirm_and_run() {
    if [ "${AUTO_YES:-0}" -eq 1 ]; then
        "$@"
    else
        read -r -p "Run '$*'? [y/N]: " a
        case "$a" in
            [Yy]*) "$@" ;;
            *) echo "Aborted"; return 1 ;;
        esac
    fi
}

show_status() {
    CURRENT_SCOPE=$(cat /proc/sys/kernel/yama/ptrace_scope)
    echo "kernel.yama.ptrace_scope = ${CURRENT_SCOPE}"
}

require_sudo() {
    if [ "$(id -u)" -ne 0 ]; then
        if ! command -v sudo >/dev/null; then
            echo "sudo not available; run as root"; exit 1
        fi
    fi
}

enable_debug() {
    require_sudo
    echo "Saving current value to ${BACKUP_FILE}"
    cat /proc/sys/kernel/yama/ptrace_scope > "${BACKUP_FILE}" || true
    sudo sysctl -w kernel.yama.ptrace_scope=0
    show_status
}

disable_debug() {
    require_sudo
    echo "Saving current value to ${BACKUP_FILE}"
    cat /proc/sys/kernel/yama/ptrace_scope > "${BACKUP_FILE}" || true
    sudo sysctl -w kernel.yama.ptrace_scope=1
    show_status
}

restore() {
    if [ -f "${BACKUP_FILE}" ]; then
        val=$(cat "${BACKUP_FILE}")
        require_sudo
        sudo sysctl -w kernel.yama.ptrace_scope="${val}"
        echo "Restored to ${val}"
    else
        echo "No backup file ${BACKUP_FILE} found"
        return 1
    fi
}

if [ "$#" -eq 0 ]; then show_help; exit 0; fi

AUTO_YES=0
case "$1" in
    --yes) AUTO_YES=1; shift ;;
esac

cmd=${1:-help}
case "$cmd" in
    on|enable)
        confirm_and_run enable_debug
        ;;
    off|disable)
        confirm_and_run disable_debug
        ;;
    status)
        show_status
        ;;
    restore)
        confirm_and_run restore
        ;;
    help|-h)
        show_help
        ;;
    *)
        echo "Unknown command: $cmd"; show_help; exit 1
        ;;
esac
