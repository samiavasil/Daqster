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
- [ ] Linux: notifier-ът се disable-ва при EOF на stdin; idle CPU ≤ ~2-3%
      без blocking stdin
- [ ] Windows: notifier-ът се disable-ва при EOF/грешка на конзолния handle
- [ ] `quit` командата през жива конзола продължава да работи
- [ ] Qt5 + Qt6 builds PASS; съществуващата test suite остава зелена
