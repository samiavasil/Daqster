# req-state.sh — единственото място, което чете състоянието на изискване.
# =============================================================================
# Source-ва се (` . scripts/req-state.sh`), не се пуска самостоятелно. Няма
# втори речник (REQ-SW-PL-108 → „Какво не се обещава“): статусите са от RDD,
# присъдата — от `plans/reviews/README.md`. Ползват го:
#   • scripts/preflight-req.sh        — вратата преди делегиране (AC1)
#   • scripts/check-commit-policy.sh  — вратата преди комит (AC2)
#
# Функции:
#   req_find <id> <root>…        път до файла (active И archive); празно = няма
#   req_field <файл> <Име>       стойността на `- **Име:** …` (празно = няма)
#   req_approved_revision <файл> номерът „ревизия N“ от реда „Одобрено“; празно
#                                = номерът още не е записан (REQ-SW-PL-106)
#   req_verdict <файл>           последната присъда от „## Ревю на черновата“
#   req_ac <файл>                „отметнати/общо“ по `- [x]` и `- [ ]`
# =============================================================================

# req_find <id> <root>… — файл по ID: първо по име, после по H1. „REQ-SW-PL-108“
# се търси и като `REQ-SW-PL-108*.md`, и като `# REQ-SW-PL-108: …`.
req_find() {
    local id="$1" root dir f
    shift
    for root in "$@"; do
        dir="$root/DevelopmentProcess/requirements"
        [ -d "$dir" ] || continue
        f="$(find "$dir" -type f \( -name 'REQ-*.md' -o -name 'DRAFT-REQ-*.md' \) \
                \( -name "$id-*.md" -o -name "$id.md" \) 2>/dev/null | sort | head -1)"
        if [ -z "$f" ]; then
            f="$(grep -rlE "^#[[:space:]]+$id([:[:space:]]|$)" \
                    --include='REQ-*.md' --include='DRAFT-REQ-*.md' "$dir" \
                    2>/dev/null | sort | head -1)"
        fi
        if [ -n "$f" ]; then
            printf '%s' "$f"
            return 0
        fi
    done
    return 1
}

# req_field <файл> <Име> — стойността на реда `- **Име:** …` (празно = няма ред).
req_field() {
    sed -n "s/^[[:space:]]*-[[:space:]]*\*\*$2:\*\*[[:space:]]*//p" "$1" 2>/dev/null \
        | head -1 | sed 's/[[:space:]]*$//'
}

# req_approved_revision <файл> — „ревизия N“ от реда „Одобрено“, ако е записан.
req_approved_revision() {
    req_field "$1" 'Одобрено' \
        | grep -oiE 'ревизия[:[:space:]]*[0-9]+' \
        | grep -oE '[0-9]+' | head -1
}

# req_verdict <файл> — последната присъда (същият awk като check-req-approval.sh:
# последният ред от таблицата печели; празно = няма присъда, тоест чака човека).
req_verdict() {
    awk '
        $0 ~ "^[[:space:]]*## Ревю на черновата[[:space:]]*$" { inSection = 1; next }
        inSection && /^[[:space:]]*## / { inSection = 0 }
        inSection && /^[[:space:]]*\|/ {
            n = split($0, cells, "|")
            if (n >= 4) {
                verdict = cells[4]
                gsub(/^[[:space:]]+|[[:space:]]+$/, "", verdict)
                if (verdict != "" && verdict != "присъда") last = verdict
            }
        }
        END { if (last != "") print last }
    ' "$1" 2>/dev/null
}

# req_ac <файл> — „отметнати/общо“: `- [x]` срещу всички критерии.
req_ac() {
    local done total
    done="$(grep -cE '^[[:space:]]*-[[:space:]]*\[[xX]\]' "$1" 2>/dev/null)"
    total="$(grep -cE '^[[:space:]]*-[[:space:]]*\[[ xX]\]' "$1" 2>/dev/null)"
    printf '%s/%s' "${done:-0}" "${total:-0}"
}
