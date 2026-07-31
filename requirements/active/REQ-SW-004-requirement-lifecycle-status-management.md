# REQ-SW-004: Requirement Lifecycle & Status Management

- **Статус:** ACTIVE
- **Приоритет:** Medium
- **Отговорник (роля):** Implementation
- **Дата:** 2026-07-31
- **Родител:** REQ-SW-001
- **Зависи от:** REQ-SW-002

## Описание

Управление на жизнения цикъл на изискванията: преход между `ACTIVE` и `DONE`/`ARCHIVE`, маркиране като завършени, автоматично преместване на файла в `archive/` и обратно при възстановяване (Reopen).

## Acceptance Criteria

- [ ] 1. Бутони/действия в UI за "Mark Done & Archive" и "Reopen".
- [ ] 2. При маркер DONE: обновяване на статуса във файла на `DONE` и преместване на `.md` файла от `active/` в `archive/`.
- [ ] 3. При Reopen: връщане обратно в `active/` и статус `ACTIVE`.
- [ ] 4. Валидация при опит за архивиране на изискване с незавършени зависимости.

## Проследимост

- **Коммити:** —
- **Код:** `src/plugins/requirements_manager/`
- **Документация:** `docs/Architecture/plugins/`
- **Тестове:** Qt5 + Qt6 builds
