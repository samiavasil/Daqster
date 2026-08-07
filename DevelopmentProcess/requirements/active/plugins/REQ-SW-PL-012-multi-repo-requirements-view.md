# REQ-SW-PL-012: Multi-Repository Requirements View (Merge)

- **Статус:** DONE
- **Приоритет:** Low
- **Отговорник (роля):** Architect + Implementation
- **Дата:** 2026-08-01
- **Родител:** REQ-SW-PL-001
- **Зависи от:** REQ-SW-PL-002, REQ-SW-PL-006, REQ-SW-PL-010

## Описание

Requirements Manager трябва да приема **ДВЕ директории с изисквания** — публичната
(`../daqster/DevelopmentProcess/requirements/` в публичното repo) и частната
(`./DevelopmentProcess/requirements/` в DaqsterAiStudio) — и да представя **обединен (merged) изглед**:

- Единно дърво/модел на изискванията с колона **"Repo"** (или секции, групирани по
  repo), указваща от коя директория идва всяко изискване.
- **Cross-tree awareness на връзките:** `Родител:`/`Зависи от:` полетата могат да
  сочат към изисквания в **другото** repo — валидаторът НЕ трябва да ги флага като
  dangling.
- Traceability matrix, покриваща **и двете дървета**.

Архитектурно правило: целият PROCESS/agent knowledge живее в частното repo
(agent-agnostic markdown), а изискванията, които засягат публичното repo, живеят в
публичното repo. Мerge изгледът е мостът между двете дървета.

**Имплементацията е завършена (2026-08-05); unit тестовете добавени 2026-08-07; статус DONE — виж Проследимост/Бележка.**

## Acceptance Criteria

- [ ] 1. Input: конфигурируеми ДВЕ директории с изисквания (public + private), всяка парсвана от `RequirementsParser`.
- [ ] 2. Merged model: обединен модел/дърво, всяко изискване носи repo идентификатор (колона/група "Repo" в UI).
- [ ] 3. Cross-repo референции (`Родител:`/`Зависи от:` към другото repo) се валидират коректно — не се отчитат като dangling.
- [ ] 4. Traceability matrix обхваща и двете дървета (REQ-SW-PL/FW/APP/BLD-* + REQ-AI/PLG/SEC).
- [ ] 5. Няма регресия на съществуващото single-directory поведение (Phase 1&2 функционалност).
- [x] 6. Покрито от unit тестове (парсване на две директории, merged model, cross-repo валидация).

## Проследимост

- **Коммити:** `c3d4e5c` (feat), `0429ea8` (test), `9d90fff` (fix: exporter CSV header — Repo column) — branch `feat/phase3-graph-matrix`
- **Код:** `src/plugins/requirements_manager/`
- **Документация:** `docs/Architecture/plugins/`
- **Тестове:** Qt5 (5.15.2) + Qt6 (6.9.2) builds + `requirements_manager_tests` — `TestMerge` 6 slots (repoForId public/private/other, two-roots merge + repo stamp, same-file-via-two-roots dedup, stable sort, empty roots, dependency hints preserved) + validator 4 slots (duplicate ID across repos, cross-repo parent not dangling, hint mismatch warning, hint match); shared binary **87/87 PASS** и на двете версии; exporter CSV header fix `9d90fff` (Repo column)

## Бележка

Имплементацията е завършена (2026-08-05); unit тестовете са добавени на
2026-08-07 след вдигане на standing инструкцията за имплементации без тестове
(комити `0429ea8`, `9d90fff`). AC 6 (Tests) е mark-нат `[x]`; верификацията
(Qt5 + Qt6 builds + unit тестове) е записана в Проследимост. Статус → **DONE**;
файлът остава в `active/` (архивирането е отделно решение, по прецедента на
PL-009/010/016).
