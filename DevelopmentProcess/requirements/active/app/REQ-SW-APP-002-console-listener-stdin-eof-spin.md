# REQ-SW-APP-002: QConsoleListener stdin-EOF busy-spin fix (idle CPU)

- **Статус:** ACTIVE
- **Приоритет:** High
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-14
- **Родител:** REQ-SW-APP-001 (архив: archive/app/REQ-SW-APP-001-daqster-application-host.md)
- **Зависи от:** REQ-SW-APP-001, REQ-SW-FW-005

## Описание

Ретроспективно изискване за вече съществуващия `QConsoleListener` в host
приложението (`src/apps/Daqster/QConsoleListener.{h,cpp}`), създадено по RDD
gate-а след установен дефект: при стартиране на Daqster без blocking stdin
(затворен/EOF stdin, например от IDE или pipe, който е приключил)
`QSocketNotifier` на `fileno(stdin)` зацикля — EOF е перманентно "readable",
notifier-ът се активира безкрайно, всяко активиране чете празен ред и емитира
празно `newLine("")`. Измерено: **181-183% idle CPU** при празен graph без
blocking stdin (perf doc `tests/performance/performance-video-display-2026-08-13.md`,
idle_cpu.txt), срещу **~1-2%** с `tail -f /dev/null |` (което държи stdin
отворен и блокиращ, без EOF и без данни → notifier-ът не се активира).

Дефектът засяга и Windows пътя (`QWinEventNotifier` на `STD_INPUT_HANDLE`) —
същият механизъм на EOF/грешка при четене трябва да спира notifier-а.

## Acceptance Criteria

- [ ] 1. **Без busy-spin при EOF.** `QConsoleListener` (Linux) не активира
      `QSocketNotifier` повече след като stdin достигне EOF; notifier-ът се
      disable-ва при първото празно/EOF четене (`feof(stdin)` / `readLine()==0`).
- [ ] 2. **Нормален режим непроменен.** При жив stdin (терминал или отворен
      pipe с данни) `quit` командата през конзолата продължава да работи
      (main.cpp:277-285); команди се четат ред по ред без загуба.
- [ ] 3. **Windows път.** `QWinEventNotifier` се disable-ва при EOF/грешка на
      конзолния handle (без зацикляне).
- [ ] 4. **CPU.** Idle CPU с празен graph без blocking stdin пада от
      ~181-183% до ≤ ~2-3% (същото ниво като blocking-stdin базовата линия).
- [ ] 5. **Builds + smoke.** Qt5 (5.15.2) + Qt6 (6.9.2) builds PASS; app smoke
      без crash (offscreen и с DISPLAY); съществуващата test suite остава
      зелена. Unit тестовете са отложени по действащата standing instruction.

## Проследимост

- **Коммити:** — (след имплементация)
- **Код:** `src/apps/Daqster/QConsoleListener.{h,cpp}`, `src/apps/Daqster/main.cpp`
- **Тестове:** отложени (standing instruction 2026-08-13)

## Бележка

Изискването е създадено **ретроспективно** (2026-08-14) по RDD gate-а —
дефектът е измерен и документиран в perf doc-а от 2026-08-13; workaround-ът
`tail -f /dev/null |` се ползва във всички перф прогони и трябва да стане
излишен.
