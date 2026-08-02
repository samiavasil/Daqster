# REQ-SW-010: Traceability Matrix View & Export

- **Статус:** DONE
- **Приоритет:** Medium
- **Отговорник (роля):** Implementation
- **Дата:** 2026-07-31
- **Родител:** REQ-SW-001
- **Зависи от:** REQ-SW-002, REQ-SW-008

## Описание

Вграден интерактивен изглед на Traceability Matrix в тула (не само експорт) + експортни формати. Матрицата показва всички изисквания (ACTIVE и DONE/archive), техните статуси, връзки (Родител/Зависи от), коммити, код и тестове — с филтриране по статус и домейн.

## Acceptance Criteria

- [x] 1. Вграден "Traceability" таб/панел в UI, показващ матрицата на живо (всички изисквания, статуси, връзки, проследимост).
- [x] 2. Филтриране на матрицата по статус (ACTIVE/DONE) и по домейн префикс.
- [x] 3. Експорт: Export to Markdown Report, Export to CSV Traceability Matrix, Export to JSON.
- [x] 4. Генериране на обобщен доклад с метрики за завършеност, статус и зависимости.
- [x] 5. Диалог за избор на файл и успешно записване на експортираните данни на диск.

## Проследимост

- **Коммити:** `a26b603` (feat) — branch `feat/phase3-graph-matrix`
- **Код:** `src/plugins/requirements_manager/`
- **Документация:** `docs/Architecture/plugins/`
- **Тестове:** Qt5 + Qt6 builds; `tests/plugins/requirements_manager/test_matrix.{h,cpp}` + `test_exporter.cpp` — 7/7 + 7/7 green (Qt5 + Qt6)
