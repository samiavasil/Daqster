---
id: PUB-002
kind: bug
status: OPEN
priority: high
created: 2026-08-14
owner:
related_req: REQ-SW-APP-002
---

## Описание

`QConsoleListener` (`src/apps/Daqster/QConsoleListener.cpp:17`) създава
`QSocketNotifier(fileno(stdin), QSocketNotifier::Read)` и го мести в отделен
`QThread`. Когато stdin е в EOF (затворен от IDE/launcher, или pipe, който е
приключил), EOF е перманентно "readable" → notifier-ът се активира безкрайно,
всяко активиране чете празен ред (`file.readLine()` връща празно) и емитира
празно `newLine("")` към main thread-а. Резултат: **busy-spin ~181-183% idle
CPU** при празен graph (измерено 2026-08-13, perf doc
`tests/performance/performance-video-display-2026-08-13.md`, idle_cpu.txt).
Работи с `tail -f /dev/null |` (държи stdin отворен и блокиращ) — този
workaround се ползва във всички перф прогони.

Засяга и Windows пътя (`QWinEventNotifier` на `STD_INPUT_HANDLE`,
QConsoleListener.cpp:14-15, 27-34).

## Acceptance Criteria
- [x] Linux: notifier-ът се disable-ва при EOF на stdin; idle CPU ≤ ~2-3%
      без blocking stdin — измерено **0.0%** instantaneous (Qt6 + Qt5,
      2026-08-14), комит `6900e2c`
- [ ] Windows: notifier-ът се disable-ва при EOF/грешка на конзолния handle
      — guard добавен (`line.empty() && cin.eof()` → disable), не е
      верифициран на реален Windows
- [x] `quit` командата през жива конзола продължава да работи — проверено
      (`printf 'quit\n' |` → "Goodbye", exit 0; `tail -f /dev/null |` →
      app работи), комит `6900e2c`. **След hoist-а** listener-ът е
      безусловен (main.cpp:185-195) → `quit` верифициран и от main app
      launcher-а (без аргументи, Qt5 + Qt6): PTY harness `quit\n` → exit
      0.12-0.17 s, exit 0; single-arg път (`NodeEditorIde`) без регресия.
      Комит: `9893d35`
- [x] Qt5 + Qt6 builds PASS; съществуващата test suite остава зелена —
      builds PASS, ctest 9/9 и на двете, комит `6900e2c`

> Статус: OPEN — заключва се след проверка от потребителя (Windows AC
> остава неверифициран).
