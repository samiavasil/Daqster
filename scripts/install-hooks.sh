#!/usr/bin/env bash
set -e
REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT" || exit 1

set -e

STRICT=0
if [ "$1" = "--strict" ]; then
	STRICT=1
fi

git config core.hooksPath scripts/hooks
chmod +x scripts/hooks/* || true
echo "Installed hooks: core.hooksPath=scripts/hooks"
if [ "$STRICT" -eq 1 ]; then
	git config --local hooks.plantumlStrict true
	echo "Hook strict mode enabled (git config hooks.plantumlStrict=true)"
else
	echo "Hook strict mode not enabled. To enable, run: ./scripts/install-hooks.sh --strict"
fi
echo "To skip hooks on a single commit, use --no-verify with git commit or git push."
