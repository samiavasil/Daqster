# REQ-FW-001: General Requirements Viewer/Editor Tool

- **Статус:** ACTIVE
- **Приоритет:** High
- **Отговорник (роля):** Architect + Implementation
- **Дата:** 2026-07-31

## Описание

Общ GUI инструмент, реализиран като **отделен application plugin** в Daqster, който
визуализира и редактира Markdown-базирани trackable requirements от `requirements/`
директории. Той е **независим от конкретен repo** — може да отвори и управлява
requirements-ите на всяко repo (публичното Daqster, частното DaqsterAiStudio и др.),
като repo-то се избира от потребителя.

Архитектура: **Parser → Model → Widget**, като цялото стои в отделен plugin
(`src/plugins/requirements_manager/`), а **не** в frame_work — следва модела на
application plugin (Interface → Object → Widget) като `NodeEditorIde`.

## Acceptance Criteria

- [x] 1. `RequirementsParser` чете Markdown `.md` файлове в `active/` и `archive/`
       и извлича: REQ ID, заглавие, статус, приоритет, отговорник, описание,
       acceptance criteria, проследимост.
- [x] 2. `RequirementsModel` (Qt Model) представя извлечените изисквания —
       ляво дърво/списък, дясно панел с детайли.
- [x] 3. Потребителят избира requirements директория (repo) през UI (default:
       текущата repo директория).
- [x] 4. **Preview Mode** — read-only преглед на детайлите на изискване.
- [x] 5. **Edit Mode** — редактор (QPlainTextEdit) с бутон Save; записва обратно в
       `.md` файла (bidirectional sync).
- [x] 6. Checkbox-ите на acceptance criteria се отразяват обратно във файла
       (`[ ]` ↔ `[x]`).
- [x] 7. Работи еднакво на Linux и Windows (без платформени специфики в кода).
- [x] 8. Компилира се и за Qt5, и за Qt6.

## Проследимост

- **Коммити:** — (не е завършено)
- **Код:** `src/plugins/requirements_manager/`
- **Документация:** `docs/Architecture/plugins/` (да се допълни)
- **Тестове:** — (Qt5 + Qt6 builds, ръчно пускане)
