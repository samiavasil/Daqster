#!/usr/bin/env bash
# check-public-docs.sh — пази публичното дърво от изтичане на частни специфики.
#
# Сканира `docs/` (всички .md) и root `*.md` за думи, които назовават **частни**
# идентификатори: частното repo/плъгин (`ai_studio`, `AiStudio`, `DaqsterAiStudio`,
# `ai-studio`, `REQ-AI-*`) и класовете на неговите нодове (`ChatNode`, `VadNode`, …).
# Договорът:
#
#   * exit 0 — няма нито едно срещание;
#   * exit 1 — има срещания; печата `file:line: term` за всяко.
#
# Няма baseline и няма ratchet: дългът е нула, затова всяко срещание е регресия.
#
# **Какво НЕ е забранено (Tier B)** — публични артефакти и семейства модели, които
# layout документът *трябва* да назове: `llama.cpp`, `whisper.cpp`, `piper`,
# `stable-diffusion.cpp`/`sd-server`, `Qwen`, `LFM`, `nomic`, както и родовите имена
# на видовете модели (`llm`, `asr`, `vlm`, `tts`, `embedding`, `sd`). Те не са частни
# — забраната им правеше `docs/Architecture/dependency-layout.md` невъзможен.
#
# Употреба:
#   ./scripts/check-public-docs.sh                 # проверка (ctest: docs_no_private_leaks)
#   ./scripts/check-public-docs.sh --list-terms    # кои думи са забранени
#   ./scripts/check-public-docs.sh --root /tmp/x   # проверка на копие на дървото
#
# Без Qt, без билд, под секунда.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

MODE="check"
while [ $# -gt 0 ]; do
    case "$1" in
        --list-terms) MODE="list" ;;
        --root)       shift; ROOT="${1:-}" ;;
        -h|--help)    sed -n '2,28p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "непознат аргумент: $1" >&2; exit 2 ;;
    esac
    shift
done

[ -d "$ROOT/docs" ] || { echo "няма $ROOT/docs" >&2; exit 2; }

# Tier A — частни идентификатори. Само те се забраняват; Tier B (трети страни и
# семейства модели) е публичен и нарочно липсва тук.
FORBIDDEN=(
    'ai_studio'
    'ai-studio'
    '\bAiStudio\b'
    '\bAI Studio\b'
    '\bDaqsterAiStudio\b'
    'REQ-AI-'
    '\bChatNode\b'
    '\bAsrNode\b'
    '\bTtsNode\b'
    '\bVlmNode\b'
    '\bOmniNode\b'
    '\bRealtimeNode\b'
    '\bVadNode\b'
    '\bImageGenNode\b'
    '\bDecisionNode\b'
    '\bMatcherNode\b'
    '\bEmbeddingNode\b'
    '\bRouterNode\b'
    '\bToolProviderNode\b'
    '\bToolMergeNode\b'
    '\bActionSinkNode\b'
    '\bEnrollNode\b'
    '\bFrameToTensorNode\b'
    '\bMatchDebugNode\b'
    '\bDecisionDebugNode\b'
    '\bToolSetDebugNode\b'
    '\bTensorDebugNode\b'
)

if [ "$MODE" = "list" ]; then
    printf '%s\n' "${FORBIDDEN[@]}"
    exit 0
fi

mapfile -t FILES < <(
    find "$ROOT/docs" -type f -name '*.md' | sort
    find "$ROOT" -maxdepth 1 -type f -name '*.md' | sort
)
[ "${#FILES[@]}" -eq 0 ] && { echo "няма .md файлове под $ROOT" >&2; exit 2; }

rel() { echo "${1#"$ROOT"/}"; }

status=0
hits=0

for f in "${FILES[@]}"; do
    for t in "${FORBIDDEN[@]}"; do
        # `-e` редове с номера; празен резултат при липса на срещане.
        matches=$(grep -Ein -- "$t" "$f" 2>/dev/null || true)
        [ -z "$matches" ] && continue
        while IFS= read -r line; do
            termlabel="${t//\\b/}"
            echo "ИЗТИЧАНЕ: $(rel "$f"):${line%%:*} — изтичане на частен идентификатор"
            echo "$(rel "$f"):${line%%:*}: $termlabel"
            echo "  $line"
            hits=$((hits + 1))
        done <<< "$matches"
        status=1
    done
done

if [ "$status" -eq 0 ]; then
    echo "проверени ${#FILES[@]} файла · забранени думи (Tier A): ${#FORBIDDEN[@]} · срещания: 0"
else
    echo "проверени ${#FILES[@]} файла · забранени думи (Tier A): ${#FORBIDDEN[@]} · срещания: $hits"
fi
exit "$status"
