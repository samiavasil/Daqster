# REQ-SW-FW-006: Process Management

- **Статус:** DONE
- **Приоритет:** P2
- **Отговорник (роля):** Implementation
- **Дата:** 2026-08-02
- **Родител:** —
- **Зависи от:** REQ-SW-FW-005

## Описание

Ретроспективно изискване за `QProcessManager` в `frame_work/base`: управление
на дъщерни процеси с предаване на instance/log параметри и групово спиране.

## Acceptance Criteria

- [x] 1. `StartProcess()` добавя `--instance-id` (QUuid 8 hex, `Id128.left(8)`),
       `--log-console-enabled`, `--log-level` и `--log-rules` (филтърни правила
       от родителския `LogManager`) към аргументите на детето.
- [x] 2. `KillAll()` прекратява всички стартирани процеси: terminate() →
       `waitForFinished(10s)` → kill() при липса на отговор; същото за
       единичен `Kill(handle)`.
- [x] 3. stdout/stderr на детето се препредават към `LogManager` (през формат,
       включващ instance ID; вече форматиран output минава директно).

## Проследимост

- **Коммити:** `5eed6d6` (feat: child process log forwarding), `bc18fa4` (feat: pass filter rules to child processes)
- **Код:** `src/frame_work/base/src/process/QProcessManager.{h,cpp}`
- **Тестове:** Qt5 + Qt6 builds
