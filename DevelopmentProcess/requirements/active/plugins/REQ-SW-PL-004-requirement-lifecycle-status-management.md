# REQ-SW-PL-004: Requirement Lifecycle & Status Management

- **Статус:** ACTIVE
- **Приоритет:** Medium
- **Отговорник (роля):** Implementation
- **Дата:** 2026-07-31
- **Родител:** REQ-SW-PL-001
- **Зависи от:** REQ-SW-PL-002

## Описание

Управление на жизнения цикъл на изискванията: преход между `ACTIVE` и `DONE`/`ARCHIVE`, маркиране като завършени, автоматично преместване на файла в `archive/` и обратно при възстановяване (Reopen).

## Acceptance Criteria

- [x] 1. Бутони/действия в UI за "Mark Done & Archive" и "Reopen".
- [x] 2. При маркер DONE: обновяване на статуса във файла на `DONE` и преместване на `.md` файла от `active/` в `archive/`.
- [x] 3. При Reopen: връщане обратно в `active/` и статус `ACTIVE`.
- [x] 4. Валидация при опит за архивиране на изискване с незавършени зависимости.

## Проследимост

- **Коммити:** —
- **Код:** `src/plugins/requirements_manager/`
- **Документация:** `docs/Architecture/plugins/`
- **Тестове:** Qt5 + Qt6 builds
