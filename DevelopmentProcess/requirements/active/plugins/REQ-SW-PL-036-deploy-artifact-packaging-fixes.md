# REQ-SW-PL-036: Deploy & Artifact-Packaging Fixes

- **Статус:** ACTIVE
- **Приоритет:** P1
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-31
- **Родител:** —
- **Зависи от:** REQ-SW-PL-035 (Version Centralization)

## Описание

Deploy/packaging pipeline-ът има два класа дефекти:

**A. AppImage packaging defects:**
1. `tools/create_appimage.sh:20` съдържа `QT_DIR=""`, който калаша env var
   `QT_DIR` преди аргумент парсинга → CI mode получава грешна Qt инсталация
   вместо aqtinstall Qt 6.8.3.
2. `release.yml` Linux job (`release.yml:14`) използва `ubuntu-22.04` + system
   Qt packages вместо `ubuntu-24.04` + aqtinstall Qt 6.8.3 (както в `ci.yml`).
   System Qt на ubuntu-22.04 е твърде стар за кодовата база.
3. OpenSSL (libssl/libcrypto) не се бандла в AppImage — QtCoinTrader plugin-а
   (`src/plugins/QtCoinTrader/CMakeLists.txt:35-36`) линква `OpenSSL::SSL` +
   `OpenSSL::Crypto` и ще fail при runtime в AppImage.
4. Няма post-build ldd верификация в `create_appimage.sh` — не се проверява
   дали всички shared library зависимости са налични в AppDir.
5. Няма Qt version check в smoke test — не се верифицира кой Qt е бил бандлнат.

**B. All-components-build failures (missing external deps):**
6. `qtwebsockets` не е инсталиран в CI — QtCoinTrader изисква
   `Qt${QT_VERSION_MAJOR}::WebSockets` (`QtCoinTrader/CMakeLists.txt:33`),
   но `ci.yml` не инсталира нито aqtinstall module `qtwebsockets` (Qt6 Linux +
   Windows), нито apt package `libqt5websockets5-dev` (Qt5 Linux).
7. `release.yml` Linux job: липсват GStreamer runtime пакети, Mesa GL пакети,
   използва се ubuntu-22.04 вместо ubuntu-24.04, и липсва aqtinstall.
8. `release.yml` Windows job (`release.yml:135`): подава `qtdeclarative qtsvg`
   като aqtinstall modules, но за Qt 6.8.3 win64_msvc2022_64 това са BASE
   пакети — aqt ще exit 1. Липсва `qtwebsockets`.
9. `print_component_status_summary()` не се извиква в `CMakeLists.txt:149`
   след `print_build_configuration_summary()`.
10. Windows/Linux images не бандлват всички external deps (Qt modules,
    GStreamer, OpenSSL, ICU) систематично.

## Acceptance Criteria

- [ ] 1. **QT_DIR fix.** `create_appimage.sh` НЕ калашва env var `QT_DIR`.
       След fix-a, `QT_DIR="$QT_ROOT_DIR" ./tools/create_appimage.sh --mode ci`
       използва правилната Qt инсталация от env var.
- [ ] 2. **release.yml Linux alignment.** `release.yml` Linux job използва
       `ubuntu-24.04` + aqtinstall Qt 6.8.3 (като `ci.yml`), НЕ system Qt
       packages. GStreamer runtime + Mesa GL пакети са добавени към apt.
- [ ] 3. **OpenSSL в AppImage.** `create_appimage.sh` бандва `libssl.so.*` и
       `libcrypto.so.*` от системата в `Daqster.AppDir/usr/lib/`.
- [ ] 4. **ldd post-build verification.** `create_appimage.sh` извършва ldd
       проверка на `usr/bin/Daqster` и на всеки plugin `.so` след бандлинг,
       и fail-ва ако някоя shared library зависимост е неизвестна.
- [ ] 5. **Qt version smoke check.** Smoke test-ът в `ci.yml` (и `release.yml`)
       вади Qt version string от AppImage и верифицира че съдържа "6.8".
- [ ] 6. **qtwebsockets в CI.** `ci.yml` Linux Qt6: `qtwebsockets` добавен
       към aqtinstall modules. `ci.yml` Linux Qt5: `libqt5websockets5-dev`
       добавен към apt packages.
- [ ] 7. **release.yml Linux fix.** `release.yml` Linux: GStreamer runtime
       (`libgstreamer1.0-0`, `gstreamer1.0-plugins-*`), Mesa GL
       (`libgl1-mesa-dri`, `libegl1`, `libgl1`, `libglx-mesa0`),
       `ubuntu-24.04`, aqtinstall Qt 6.8.3.
- [ ] 8. **release.yml Windows fix.** `release.yml` Windows: премахнати
       `qtdeclarative qtsvg` от modules; добавено `qtwebsockets`.
- [ ] 9. **Component status summary.** `CMakeLists.txt:149` — добавено
       `print_component_status_summary()` след `print_build_configuration_summary()`.
- [ ] 10. **Bundled external deps.** Windows portable ZIP и Linux AppImage
        съдържат всички required deps: Qt modules (QtWebSockets вкл.),
        GStreamer (Linux), OpenSSL (libssl + libcrypto), ICU.
- [ ] 11. **Builds + smoke.** Qt5 + Qt6 builds PASS; ctest 28/28 green;
        AppImage smoke PASS с верифицирана Qt версия; Windows smoke PASS.

## Проследимост

- **Коммити:** — (след имплементация)
- **Код:** `tools/create_appimage.sh`, `.github/workflows/ci.yml`,
  `.github/workflows/release.yml`, `CMakeLists.txt:149`
- **Документация:** `CHANGELOG.md`
- **Тестове:** Qt5 + Qt6 builds + ctest + AppImage smoke + Windows smoke
