#!/usr/bin/env bash
set -e
REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT" || exit 1

git config core.hooksPath scripts/hooks
chmod +x scripts/hooks/* || true
echo "Installed hooks: core.hooksPath=scripts/hooks"
echo "To skip hooks on a single commit, use --no-verify with git commit or git push."
