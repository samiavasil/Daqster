#!/usr/bin/env bash
# check-scene-paths.sh — сцените в repo-то не носят машинни пътища (REQ-SW-PL-091).
#
# Защо: `.flow` файлът е артефакт в git. Ако поле-път съдържа абсолютен път
# (`/mnt/...`, `/home/...`, `C:\...`) или `${DAQSTER_DEPS}`/`${HOME}`, сцената
# работи на точно една машина: на чиста машина тя гледа в несъществуващ файл, а
# човекът не вижда защо — сцената „просто не тръгва".
#
# Правилото за поле-път (`internal-data` ключ, завършващ на `Path`/`File`/
# `Binary`) е: стойността е
#   • празна („няма override" — нормалното състояние за фикстура), или
#   • референция/относително име (`../clip.mp4`, `sample_48k.wav` — закача се за
#     директорията на сцената през `SceneVariables::resolvePath()`), или
#   • `${DAQSTER_VIDEO_FILE:-<име>}` — **входен** override с относителна стойност
#     по подразбиране (`knownTokenNames()` в `src/plugins/common/Scene/`).
# Всяко друго — абсолютен път, `${DAQSTER_DEPS}`, `${HOME}` — е грешка.
#
# Обхват: **само файлове в repo-то** (`git ls-files '*.flow'`). Сцена извън
# repo-то е на потребителя — там абсолютен път е негово решение и не се съди
# (`--dir <dir>` проверява такава директория само когато човек поиска).
#
# Едно изключение по име: `malformed.flow` е **нарочно** невалиден JSON
# (негативна фикстура — `kNegativeFixtures` в
# `tests/plugins/node_editor_ide/test_flow_fixtures.cpp`); той не се парсва, за да
# го види fixture тестът, и не е сцена, която някой пуска.
#
# Нищо не се зарежда и нищо не се стартира: JSON се чете, под секунда е, без
# Qt и без билд — затова върви и в ниво 0.
#
# Usage:
#   ./scripts/check-scene-paths.sh              # всички `.flow` в repo-то
#   ./scripts/check-scene-paths.sh --quiet      # само грешките
#   ./scripts/check-scene-paths.sh --dir <dir>  # друга директория (`*.flow` под нея)
#
# Exit: 0 = чисто; 1 = има поле-път с машинна стойност; 2 = липсва python3.

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QUIET=""
DIR=""

while [ $# -gt 0 ]; do
    case "$1" in
        --quiet) QUIET=1 ;;
        --dir)   shift; DIR="${1:-}" ;;
        -h|--help)
            sed -n '2,40p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *) echo "непознат аргумент: $1" >&2; exit 2 ;;
    esac
    shift
done

command -v python3 >/dev/null 2>&1 || {
    echo "✗ липсва python3 — проверката не може да прочете JSON-а" >&2
    exit 2
}

cd "$REPO_ROOT" || exit 2

if [ -n "$DIR" ]; then
    [ -d "$DIR" ] || { echo "✗ липсва директорията $DIR" >&2; exit 2; }
    # Извън repo-то няма git списък: обхожда се директорията.
    mapfile -t FILES < <(find "$DIR" -name '*.flow' -type f -print | sort)
else
    # Обхватът е това, което git познава: временни/билднати копия не се съдят.
    mapfile -t FILES < <(git ls-files -- '*.flow')
fi

[ "${#FILES[@]}" -eq 0 ] && { echo "✓ няма .flow файлове за проверка"; exit 0; }

python3 - "$QUIET" "${FILES[@]}" <<'PY'
import json
import os
import re
import sys

quiet = sys.argv[1] == "1"
paths = sys.argv[2:]

# Нарочно счупени фикстури: не се парсват и не са сцени (виж `kNegativeFixtures`).
NEGATIVE = {"malformed.flow"}

# Поле-път: същият набор, който нодовете разширяват (`expandEnv`) или резолвват.
PATH_KEY = re.compile(r'(Path|File|Binary)$')
EXTRA_KEYS = {"voiceModel"}
# Токени, които носят **корен**, а не име: тяхната стойност е машинна.
ROOT_TOKENS = ("DAQSTER_DEPS", "HOME", "USERPROFILE")
ABS_WIN = re.compile(r'^[A-Za-z]:[\\/]')


def is_path_key(key: str) -> bool:
    return bool(PATH_KEY.search(key)) or key in EXTRA_KEYS


def verdict(value: str):
    """Връща причината, ако стойността е машинна; иначе None."""
    if not value:                       # празно = няма override
        return None
    if ABS_WIN.match(value):
        return "абсолютен път (Windows)"
    if value.startswith("/"):
        return "абсолютен път"
    for token in ROOT_TOKENS:
        if "${%s" % token in value:
            return "машинен корен ${%s}" % token
    if not value.startswith("${"):
        return None                     # референция или относително име
    # Токен: `${ИМЕ}`, `${ИМЕ:-default}`, `${ИМЕ:?подсказка}`.
    match = re.match(r'\$\{([A-Za-z_][A-Za-z0-9_]*)(:[-?][^}]*)?\}', value)
    if not match:
        return "неразпознат токен"
    name, rest = match.group(1), match.group(2) or ""
    tail = value[match.end():]
    if rest.startswith(":-"):
        default = rest[2:]
        if default.startswith("/") or ABS_WIN.match(default):
            return "абсолютна стойност по подразбиране"
        if not tail:
            return None                 # `${X:-име}` = входен override
    if not tail:
        return None                     # `${VAR}` без път — стойността идва отвън
    return "машинен път през ${%s}" % name


problems = []
for path in paths:
    if os.path.basename(path) in NEGATIVE:
        continue
    try:
        with open(path, "r", encoding="utf-8") as handle:
            document = json.load(handle)
    except (OSError, ValueError) as error:
        problems.append((path, "-", str(error)))
        continue
    for node in document.get("nodes", []):
        node_id = node.get("id", "?")
        data = node.get("internal-data", {}) or {}
        model = data.get("model-name", "?")
        for key, value in data.items():
            if not isinstance(value, str) or not is_path_key(key):
                continue
            reason = verdict(value)
            if reason:
                problems.append((path, "%s (%s).%s" % (node_id, model, key),
                                 "%s — %s" % (value, reason)))

if problems:
    print("✗ машинни пътища в комитирани сцени (%d):" % len(problems))
    for path, where, detail in problems:
        print("  %s:%s: %s" % (path, where, detail))
    print()
    print("  Поле-път е override за човека: празно поле, относително име до сцената")
    print("  или `${DAQSTER_VIDEO_FILE:-<име>}` за вход.")
    sys.exit(1)

if not quiet:
    print("✓ сцените в repo-то: %d файла, 0 машинни пътя" % len(paths))
PY
