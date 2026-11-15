#!/usr/bin/env bash
# check-public-docs.sh — пази публичното дърво от изтичане на частни специфики.
#
# Сканира `docs/` (всички .md) и root `*.md` за думи, които описват частен
# продукт/плъгин и не бива да се появяват в публичната документация. Договорът:
#
#   * exit 0 — няма нови срещания;
#   * exit 1 — има нови срещания; печата `file:line: term` за всяко.
#
# „Нови“ значи: (файл, дума) двойка, която не е в `scripts/public-docs-baseline.txt`,
# или брой, по-голям от замразения. Baseline-ът е **само за намаляване**: той
# описва какво е имало преди пазача, не разрешение. Расте ли, това е регресия.
#
# Употреба:
#   ./scripts/check-public-docs.sh                    # проверка (ctest: docs_no_private_leaks)
#   ./scripts/check-public-docs.sh --list-baseline     # какво е замразено
#   ./scripts/check-public-docs.sh --update-baseline   # презаписва baseline-а (внимателно!)
#   ./scripts/check-public-docs.sh --root /tmp/x       # проверка на копие на дървото
#
# Без Qt, без билд, под секунда.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BASELINE="$SCRIPT_DIR/public-docs-baseline.txt"

MODE="check"
while [ $# -gt 0 ]; do
    case "$1" in
        --update-baseline) MODE="update" ;;
        --list-baseline)   MODE="list" ;;
        --root)            shift; ROOT="${1:-}" ;;
        -h|--help)         sed -n '2,25p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "непознат аргумент: $1" >&2; exit 2 ;;
    esac
    shift
done

[ -d "$ROOT/docs" ] || { echo "няма $ROOT/docs" >&2; exit 2; }

# Забранени думи. Граници на думите там, където имат смисъл (къси думи като
# `tts`/`vlm`/`asr` иначе биха хванали произволен подниз).
FORBIDDEN=(
    'ai_studio'
    'AiStudio'
    'AI Studio'
    'REQ-AI-'
    'DaqsterAiStudio'
    '\bllama\b'
    '\bwhisper\b'
    '\bpiper\b'
    'stable-diffusion'
    'sd-server'
    '\bvlm\b'
    '\bomni\b'
    '\btts\b'
    '\basr\b'
    '\bembedding\b'
    '\bQwen\b'
    '\bLFM\b'
)

mapfile -t FILES < <(
    find "$ROOT/docs" -type f -name '*.md' | sort
    find "$ROOT" -maxdepth 1 -type f -name '*.md' | sort
)
[ "${#FILES[@]}" -eq 0 ] && { echo "няма .md файлове под $ROOT" >&2; exit 2; }

rel() { echo "${1#"$ROOT"/}"; }

# Наблюдавани бройки: "<relative path>\t<term>\t<count>"
observed() {
    local f t c
    for f in "${FILES[@]}"; do
        for t in "${FORBIDDEN[@]}"; do
            c=$(grep -Eic -- "$t" "$f" 2>/dev/null || true)
            [ -z "$c" ] && c=0
            [ "$c" -gt 0 ] && printf '%s\t%s\t%s\n' "$(rel "$f")" "$t" "$c"
        done
    done
}

if [ "$MODE" = "update" ]; then
    mkdir -p "$(dirname "$BASELINE")"
    {
        echo "# Замразени срещания на забранени думи в публичната документация."
        echo "# Формат: <път>\t<regex>\t<брой>. Само за НАМАЛЯВАНЕ — расте ли ред,"
        echo "# това е регресия; нов ред значи ново изтичане. Регенерира се с"
        echo "# ./scripts/check-public-docs.sh --update-baseline"
        observed | sort
    } > "$BASELINE"
    echo "baseline: $(grep -vc '^#' "$BASELINE") записа(ни) в $(rel "$BASELINE")"
    exit 0
fi

if [ "$MODE" = "list" ]; then
    [ -f "$BASELINE" ] && grep -v '^#' "$BASELINE" || echo "(празен baseline)"
    exit 0
fi

# Проверка: замразеният брой е таванът за (файл, дума).
declare -A CAP=()
if [ -f "$BASELINE" ]; then
    while IFS=$'\t' read -r p t c; do
        case "$p" in ''|'#'*) continue ;; esac
        CAP["$p"$'\x1f'"$t"]="$c"
    done < <(grep -v '^#' "$BASELINE")
fi

status=0
over=0
frozen=0
declare -A SEEN=()

while IFS=$'\t' read -r p t c; do
    [ -z "$p" ] && continue
    cap="${CAP["$p"$'\x1f'"$t"]:-0}"
    SEEN["$p"$'\x1f'"$t"]=1
    if [ "$c" -gt "$cap" ]; then
        over=$((over + 1))
        echo "ИЗТИЧАНЕ: $p — '$t' ×$c (замразени: $cap)"
        grep -Ein -- "$t" "$ROOT/$p" | sed "s|^|  $p:|"
        status=1
    else
        frozen=$((frozen + 1))
    fi
done < <(observed)

echo "проверени ${#FILES[@]} файла · забранени думи: ${#FORBIDDEN[@]} · замразени двойки (файл, дума): $frozen · нови: $over"
[ "$frozen" -gt 0 ] && echo "  (замразеното е дълг, не разрешение — виж $(rel "$BASELINE"); целта е 0)"
exit "$status"
