# REQ-SW-PL-035: Version Centralization — Single Source of Truth & Semver Bump Check

- **Статус:** ACTIVE
- **Приоритет:** P1
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-30
- **Родител:** —
- **Зависи от:** REQ-SW-BLD-001 (CMake Plugin Build Infrastructure), REQ-SW-FW-001 (Plugin Manager Core)

## Описание

Версията на Daqster в момента е **фрагментирана**: root `CMakeLists.txt` декларира
`project(Daqster VERSION 0.2.0)`, а всеки plugin хардкодира собствена версия в
`*Interface.cpp` чрез `SetProperty(PLUGIN_VERSION, ...)` (напр.
`DemoNodeEditorNodesInterface.cpp:12` → `"0.2.0"`, `QtCoinTraderInterface.cpp:14` →
`"0.0.1"`, тестовите плъгини → `"0.0.1"`). Няма единен източник на истина и няма
semver bump проверка при release.

Това изискване централизира версията в **два компонента** (третият — runtime
plugin version check — е бъдещ лост, виж Бележки):

1. **Централна версия (single source of truth):** нов `VERSION` файл в корена на
   публичното repo → генериран `daqster_version.h` (с `DAQSTER_VERSION_MAJOR/MINOR/PATCH`
   и `DAQSTER_VERSION_STRING`) → `create_plugin()` (в `cmake/ComponentTemplates.cmake`)
   разпространява версията до всички плъгини (вкл. legacy QtCoinTrader + тестовите
   плъгини) чрез include dir + compile definition. Release версията е **v0.3.0** —
   всички плъгини се вдигат на `0.3.0`.

2. **Release-time semver bump check:** скрипт `suggest_version.sh` чете текущата
   версия от `VERSION` файла, инспектира git историята (типовете комити от последния
   tag) и предлага следващата major/minor/patch версия по semver правилата.

## Acceptance Criteria

- [ ] 1. **`VERSION` файл.** Нов файл `VERSION` в корена на публичното repo съдържа
       `0.3.0` като единствен източник на истина.
- [ ] 2. **Root `project()` от `VERSION`.** Root `CMakeLists.txt` чете версията от
       `VERSION` файла (не хардкодира `0.2.0`).
- [ ] 3. **Генериран `daqster_version.h`.** При configure се генерира
       `daqster_version.h` с `DAQSTER_VERSION_MAJOR/MINOR/PATCH` и
       `DAQSTER_VERSION_STRING`, съответстващи на `VERSION` файла.
- [ ] 4. **`create_plugin()` разпространява версията.** `create_plugin()` в
       `cmake/ComponentTemplates.cmake` добавя include dir-а на `daqster_version.h`
       + compile definition към всеки plugin target, така че плъгините могат да
       реферират централната версия.
- [ ] 5. **Всички плъгини на 0.3.0.** Всички плъгини (вкл. legacy QtCoinTrader +
       тестовите плъгини) репортват `0.3.0` в runtime; няма хардкодирани version
       strings в `*Interface.cpp`.
- [ ] 6. **`suggest_version.sh`.** Скриптът чете текущата версия от `VERSION`,
       инспектира git log от последния tag, класифицира комитите (feat → minor,
       fix → patch, breaking → major) и предлага следващата semver версия с
       обосновка.
- [ ] 7. **Builds + smoke.** Qt5 + Qt6 builds PASS; app smoke без crash;
       съществуващата test suite остава зелена.

## Проследимост

- **Коммити:** — (след имплементация)
- **Код:** `VERSION`, `CMakeLists.txt`, `cmake/ComponentTemplates.cmake`,
  `scripts/suggest_version.sh`, всички `*Interface.cpp` файлове на плъгините
- **Документация:** `docs/Architecture/BuildSystemArchitecture.md`,
  `docs/Architecture/framework/QPluginManager.md`, `CHANGELOG.md`
- **Тестове:** unit тестове за `suggest_version.sh` класификация

## Бележки по имплементацията (план)

- **`create_plugin()` hook:** `cmake/ComponentTemplates.cmake:83-156` — добавяне на
  include dir + compile definition е **additive** (не чупи съществуващите плъгини).
- **Semver сравнение:** трябва да е major/minor/patch-aware, не string сравнение.
- **Бъдещ лост (FUTURE):** Runtime plugin version check при зареждане —
  `QPluginManager::LoadPluginInterfaceObject()` (`src/frame_work/base/src/QPluginManager.cpp:389-439`)
  да сравнява версията на plugin-а (`QPluginInterface::GetVersion()`) с версията на
  framework-а (от `daqster_version.h`), да логва/маркира несъответствие, и при
  няколко версии на един plugin да приоритизира най-близката до framework-а.
  Това е отделна бъдеща имплементация (пипане в ядрото на plugin loading-а).

## Бележка

Изискването е създадено **преди** имплементацията (2026-08-30) по одобреното
решение за централизация на версията. Release версията е **v0.3.0**; всички
плъгини (вкл. legacy QtCoinTrader + тестовите) се вдигат на `0.3.0`. Компонентите
(централна версия + semver bump check) се имплементират в една верига; runtime
plugin version check е бъдещ лост.