#!/usr/bin/env bash
# Инсталира versioned git hooks от scripts/hooks/ (idempotent).
#
#   ./scripts/install-hooks.sh            # core.hooksPath=scripts/hooks
#   ./scripts/install-hooks.sh --strict   # + hooks.plantumlStrict=true
#
# Hooks: pre-commit (PlantUML рендер) и pre-push (master приема само от
# develop/release/* — виж scripts/hooks/pre-push).
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

STRICT=0
if [ "${1:-}" = "--strict" ]; then
	STRICT=1
fi

previous="$(git config --local --get core.hooksPath || true)"
if [ "$previous" = "scripts/hooks" ]; then
	echo "Hooks: core.hooksPath вече е 'scripts/hooks' — без промяна."
else
	git config --local core.hooksPath scripts/hooks
	echo "Hooks: core.hooksPath '${previous:-<няма>}' → 'scripts/hooks'."
fi

chmod +x scripts/hooks/*
echo "Hooks: изпълними — $(cd scripts/hooks && echo *)"

if [ "$STRICT" -eq 1 ]; then
	if [ "$(git config --local --get hooks.plantumlStrict || true)" = "true" ]; then
		echo "Hooks: strict mode вече е включен (hooks.plantumlStrict=true)."
	else
		git config --local hooks.plantumlStrict true
		echo "Hooks: strict mode включен (hooks.plantumlStrict=true) — липсващ PlantUML спира commit."
	fi
else
	echo "Hooks: strict mode не е включен. За да го включиш: ./scripts/install-hooks.sh --strict"
fi

echo "Hooks: pre-push пази master — пуска се само за refs/heads/master, другите ref-ове минават."
echo "Hooks: за да прескочиш еднократно — git commit --no-verify / git push --no-verify."
