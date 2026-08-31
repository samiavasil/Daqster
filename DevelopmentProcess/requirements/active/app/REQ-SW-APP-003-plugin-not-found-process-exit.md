# REQ-SW-APP-003: Plugin-not-found startup — process must exit (no empty window)

- **Статус:** ACTIVE
- **Приоритет:** Medium
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-14
- **Родител:** REQ-SW-APP-001 (архив: archive/app/REQ-SW-APP-001-daqster-application-host.md)
- **Зависи от:** REQ-SW-APP-002 (хостване на QConsoleListener)

## Описание

При стартиране `Daqster <plugin-name>` (single-arg) с несъществуващ plugin име,
`main.cpp` не намира match (нито по hash, нито по име) и показва
`QMessageBox::critical` („Application plugin ... was not found"), но след това
кодът продължава към `a.exec()` — процесът остава жив като ПРАЗЕН прозорец без
никаква функционалност, докато потребителят не го затвори ръчно. Същото важи
при намерен hash, но неуспешен `CreatePluginObject` (nullptr).

Очакване: след показаната грешка процесът се затваря САМ (exit code != 0) —
няма нужда да стои празен. Това е особено важно за child процесите, спавнати
от главния launcher (`ApplicationsManager::StartApplication`) — не трябва да
остават висящи безсмислено при грешка в името.

## Acceptance Criteria

- [x] 1. `Daqster <неизвестен-plugin>` (Qt5 и Qt6): показва се грешката и
      процесът ИЗЛИЗА сам (без празен прозорец), exit code != 0.
- [x] 2. Child процес от launcher-а (multi-arg / `StartApplication`) с
      несъществуващ plugin също излиза сам.
- [x] 3. `Daqster NodeEditorIde` (валиден plugin) работи както преди — без
      регресия; quit през конзолата продължава да работи (REQ-SW-APP-002).
- [x] 4. Qt5 + Qt6 builds PASS; съществуващата test suite остава зелена.
      Unit тестовете са отложени по действащата standing instruction.
- [x] 5. Без busy-spin / ненужен CPU при неизвестен plugin (EOF fix-ът от
      REQ-SW-APP-002 остава валиден).

## Проследимост

- **Коммити:** `chore (REQ-SW-APP-003)` — REQ файл; `fix (REQ-SW-APP-003)` —
  `return 1;` в single-arg failure path-ите; `docs (REQ-SW-APP-003)` — changelog
  + AC updates.
- **Код:** `src/apps/Daqster/main.cpp` (single-arg branch: not-found + create-failure paths)
- **Тестове:** отложени (standing instruction 2026-08-13). Верификация: ръчна
  (Qt5 + Qt6, DISPLAY=:0, xsendreturn/xraise_return XTest helper за затваряне на
  модалния диалог): exit code 1 за невалиден plugin; child от multi-arg launcher
  също излиза с 1; `NodeEditorIDE` стартира и `quit` през конзолата работи;
  ctest 9/9 (Qt5 + Qt6).

## Бележка

Създадено (2026-08-14) по инициатива на потребителя след наблюдение, че
spawn-нат процес с несъществуващ plugin виси празен, докато не се кликне OK на
грешката и не се затвори прозорецът ръчно.
