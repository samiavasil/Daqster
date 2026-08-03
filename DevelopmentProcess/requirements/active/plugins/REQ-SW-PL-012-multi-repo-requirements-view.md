# REQ-SW-PL-012: Multi-Repository Requirements View (Merge)

- **Статус:** BACKLOG
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

**Това е future feature — НЕ се имплементира в текущата итерация.**

## Acceptance Criteria (за бъдещата имплементация — not now)

- [ ] 1. Input: конфигурируеми ДВЕ директории с изисквания (public + private), всяка парсвана от `RequirementsParser`.
- [ ] 2. Merged model: обединен модел/дърво, всяко изискване носи repo идентификатор (колона/група "Repo" в UI).
- [ ] 3. Cross-repo референции (`Родител:`/`Зависи от:` към другото repo) се валидират коректно — не се отчитат като dangling.
- [ ] 4. Traceability matrix обхваща и двете дървета (REQ-SW-PL/FW/APP/BLD-* + REQ-AI/PLG/SEC).
- [ ] 5. Няма регресия на съществуващото single-directory поведение (Phase 1&2 функционалност).
- [ ] 6. Покрито от unit тестове (парсване на две директории, merged model, cross-repo валидация).

## Проследимост

- **Коммити:** —
- **Код:** `src/plugins/requirements_manager/`
- **Документация:** `docs/Architecture/plugins/`
- **Тестове:** —
