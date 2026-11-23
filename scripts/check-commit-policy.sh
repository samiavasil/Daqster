#!/usr/bin/env bash
# check-commit-policy.sh — какво не влиза в repo-то (REQ-SW-PL-108, AC2).
# =============================================================================
#   ./scripts/check-commit-policy.sh [--root DIR] [--message-file FILE|--no-message]
#                                    [--skip-shared-stamp] [--quiet]
#
# Отказва, когато:
#   (а) staged diff-ът пипа код, а в съобщението няма цитиран `REQ-…`;
#   (б) цитирано изискване не е ACTIVE/IMPLEMENTED/DONE (DRAFT, CANCELLED,
#       BACKLOG), ревизията му не е одобрена или последната му присъда е
#       „за корекция“ (чете се от `scripts/req-state.sh`, както `preflight-req.sh`);
#       това се проверява САМО когато комитът пипа код (виж (а)) — иначе
#       цитираният ID е информативен, за да е комитваемо създаването на черновата;
#   (в) staged diff-ът пипа споделен път, а `.test-evidence/1-<HEAD short>.txt`
#       липсва или няма `scope full`.
# `docs:`/`chore:` комит, който не пипа код, минава без REQ и без проверка на
# статуса на цитираните ID-та (черновата се създава с docs комит преди присъдата).
#
# Вика се от двете куки (`scripts/install-hooks.sh` слага `core.hooksPath`):
#   `.githooks/pre-commit`  → `--no-message`: git още НЕ е записал съобщението
#                             (`git commit -m` го създава след pre-commit),
#                             затова там важи само правило (в);
#   `.githooks/commit-msg`  → `--message-file "$1"`: git подава пътя, важат (а) и
#                             (б) и комитът се отказва преди обекта.
# Двете заедно са вратата; `--no-verify` прескача и двете, затова има политика
# (AC3) и Gitea pre-receive (AC5).
#
# „Код“ са `src/`, `plugins/`, `tests/` (без `.flow` сцените в `tests/data/`),
# `cmake/` и всеки `CMakeLists.txt` — същите дървета, които `check-level1-scope.sh`
# познава. `scripts/` НЕ е код: скриптовете са инструментът на процеса, не
# продуктът, и се променят и с `chore:`.
#
# Файлът е един и същ в двете repo-та (частното и публичното) — поведението не
# се разминава, затова и `req-state.sh` се копира наравно с него.
#
# Изход: 0 = комитът е законен, 1 = не е (с едно изречение какво липсва), 2 = употреба.
# =============================================================================
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MSG_FILE=""
NO_MSG=0
SKIP_SHARED=0
QUIET=0
while [ $# -gt 0 ]; do
    case "$1" in
        --root) ROOT="${2:?--root иска директория}"; shift 2 ;;
        --message-file) MSG_FILE="${2:?--message-file иска файл}"; shift 2 ;;
        --no-message) NO_MSG=1; shift ;;
        --skip-shared-stamp) SKIP_SHARED=1; shift ;;
        --quiet) QUIET=1; shift ;;
        -h|--help) sed -n '2,33p' "$0"; exit 0 ;;
        *) echo "непознат аргумент: $1" >&2; exit 2 ;;
    esac
done

gitdir="$(git -C "$ROOT" rev-parse --absolute-git-dir 2>/dev/null)" || {
    [ "$QUIET" -eq 1 ] || echo "✓ $ROOT не е git repo — политиката не се прилага"
    exit 0
}
[ -n "$MSG_FILE" ] || MSG_FILE="$gitdir/COMMIT_EDITMSG"
if [ "$NO_MSG" -eq 0 ] && [ ! -f "$MSG_FILE" ]; then
    echo "✖ липсва съобщение на комита: $MSG_FILE" >&2
    exit 1
fi

# shellcheck source=scripts/req-state.sh
. "$ROOT/scripts/req-state.sh"

STAGED="$(git -C "$ROOT" diff --cached --name-only 2>/dev/null)"
CODE_FILES="$(printf '%s\n' "$STAGED" | while IFS= read -r f; do
    [ -n "$f" ] || continue
    case "$f" in
        tests/data/*.flow) continue ;;                        # сцена, не код
        src/*|plugins/*|tests/*|cmake/*) printf '%s\n' "$f" ;;
        *) [ "$(basename "$f")" = "CMakeLists.txt" ] && printf '%s\n' "$f" ;;
    esac
done)"
IDS=""
[ "$NO_MSG" -eq 1 ] || IDS="$(grep -oE 'REQ-[A-Z-]+-[0-9]+' "$MSG_FILE" 2>/dev/null | sort -u)"

# (а) код без цитиран REQ. `--no-message` значи „съобщението още не съществува“ —
# правилото важи в commit-msg, не във pre-commit.
if [ "$NO_MSG" -eq 0 ] && [ -n "$CODE_FILES" ] && [ -z "$IDS" ]; then
    echo "✖ код без цитиран REQ: комитът пипа" >&2
    printf '%s\n' "$CODE_FILES" | head -5 | sed 's/^/    /' >&2
    echo "  добави „REQ-…“ в съобщението (или раздели промяната на docs/chore)" >&2
    exit 1
fi

# (б) състоянието на всяко цитирано изискване, САМО за комит, който пипа код:
# docs:/chore: комит без код само създава или описва изискване (вкл. черновата
# преди присъда), затова там цитираният ID е информативен и не се съди по статус.
# Търси се и в съседното repo — изискванията на публичния код живеят в частното
# (`REQ-SW-PL-*`).
CANDIDATE_ROOTS=("$ROOT")
for sib in daqster DaqsterAiStudio; do
    cand="$(dirname "$ROOT")/$sib"
    [ "$cand" != "$ROOT" ] && [ -d "$cand" ] && CANDIDATE_ROOTS+=("$cand")
done
if [ -n "$CODE_FILES" ]; then
for id in $IDS; do
    if ! file="$(req_find "$id" "${CANDIDATE_ROOTS[@]}")"; then
        [ "$QUIET" -eq 1 ] || echo "⚠ $id — няма файл в това repo, нито в съседното; статусът не е проверен" >&2
        continue
    fi
    status="$(req_field "$file" 'Статус')"
    status="${status%%[[:space:]]*}"
    revision="$(req_field "$file" 'Ревизия')"
    approved="$(req_approved_revision "$file")"
    verdict="$(req_verdict "$file")"
    show="$file"
    for cand in "${CANDIDATE_ROOTS[@]}"; do show="${show#"$cand"/}"; done
    # AC2 казва „отказва се DRAFT“, не „иска се ACTIVE“: поправка на вече прието
    # изискване (IMPLEMENTED, DONE) е законна работа и се цитира. Отказват се само
    # състоянията, в които договорът не е в сила — чернова, отказано, backlog.
    case "$status" in
        ACTIVE|IMPLEMENTED|DONE) ;;
        DRAFT)
            echo "✖ $id е DRAFT ($show) — не се цитира от код" >&2
            echo "  черновата чака присъда на човека" >&2
            exit 1 ;;
        *)
            echo "✖ $id е ${status:-без статус} ($show) — не се цитира от код" >&2
            echo "  цитира се само ACTIVE, IMPLEMENTED или DONE" >&2
            exit 1 ;;
    esac
    if [ -n "$revision" ] && [ -n "$approved" ] && [ "$revision" != "$approved" ]; then
        echo "✖ $id: ревизия $revision не е одобрена (одобрена: $approved)" >&2
        exit 1
    fi
    if [ "$verdict" = "за корекция" ]; then
        echo "✖ $id: последната присъда е „за корекция“ — първо се коригира изискването" >&2
        exit 1
    fi
done
fi

# (в) споделен път без печат `scope full` за HEAD.
if [ "$SKIP_SHARED" -eq 0 ]; then
    SHARED="$(printf '%s\n' "$STAGED" | grep -E '^(src/frame_work/|src/plugins/common/|cmake/|CMakeLists\.txt$)' | head -1)"
    if [ -n "$SHARED" ]; then
        short="$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || true)"
        stamp="$ROOT/.test-evidence/1-$short.txt"
        if [ -z "$short" ] || [ ! -f "$stamp" ] \
           || ! grep -q '^result[[:space:]]*PASS' "$stamp" \
           || ! grep -q '^scope[[:space:]]*full' "$stamp"; then
            echo "✖ споделен път $SHARED — пусни: ./scripts/test-level.sh 1 --full" >&2
            echo "  печатът за HEAD (${short:-?}) е ${stamp#"$ROOT"/} с „result PASS“ и „scope full“" >&2
            exit 1
        fi
    fi
fi

if [ "$QUIET" -eq 0 ]; then
    nfiles="$(printf '%s\n' "$STAGED" | grep -c .)"
    if [ "$NO_MSG" -eq 1 ]; then
        printf '✓ индексът е законен (%s файла; съобщението се проверява в commit-msg)\n' "$nfiles"
    else
        printf '✓ комитът е законен (%s цитирани REQ, %s файла)\n' \
            "$(printf '%s\n' "$IDS" | grep -c .)" "$nfiles"
    fi
fi
exit 0
