# REQ-SW-PL-001: Requirements Viewer/Editor Tool (Base)

- **Статус:** ACTIVE
- **Приоритет:** High
- **Отговорник (роля):** Architect + Implementation
- **Дата:** 2026-07-31
- **Родител:** —
- **Зависи от:** —

## Описание

Общ GUI инструмент, реализиран като **отделен application plugin** в Daqster (`src/plugins/requirements_manager/`), който визуализира и редактира Markdown-базирани trackable requirements от `DevelopmentProcess/requirements/` директории. Инструментът е независим от конкретен repository и позволява избор на работна директория.

## Acceptance Criteria

- [x] 1. `RequirementsParser` чете Markdown `.md` файлове в `active/` и `archive/`.
- [x] 2. `RequirementsModel` (Qt Model) представя извлечените изисквания.
- [x] 3. Избор на requirements директория през UI.
- [x] 4. **Preview Mode** — read-only преглед на детайлите.
- [x] 5. **Edit Mode** — редактор с бутон Save (bidirectional sync).
- [x] 6. Cross-platform поддръжка (Linux & Windows).
- [x] 7. Компилира се за Qt5 и Qt6.

## Проследимост

- **Коммити:** —
- **Код:** `src/plugins/requirements_manager/`
- **Документация:** `docs/Architecture/plugins/`
- **Тестове:** Qt5 + Qt6 builds
