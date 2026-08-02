# REQ-SW-024: Unit Test Infrastructure

- **Статус:** DONE
- **Приоритет:** P2
- **Отговорник (роля):** Implementation
- **Дата:** 2026-08-02
- **Родител:** —
- **Зависи от:** REQ-SW-023

## Описание

Ретроспективно изискване за unit test инфраструктурата: тестови плъгини
(plugin_main_test, plugin_fancy_test, plugin_uggly_test, template_plugin_daqster)
в `src/plugins/tests/`, тестовите binaries за Requirements Manager
(headless + GUI) и добавянето им като CTest targets.

## Acceptance Criteria

- [x] 1. Тестови плъгини са налични в `src/plugins/tests/` (а не
       `tests/plugins/tests/`): `plugin_main_test`, `plugin_fancy_test`,
       `plugin_uggly_test`, `template_plugin_daqster` — включени от root
       `CMakeLists.txt` (`add_subdirectory(tests)`).
- [x] 2. `tests/plugins/requirements_manager/CMakeLists.txt` дефинира headless
       binaries: `requirements_manager_tests` (test_main.cpp) + отделни
       `requirements_manager_matrix_tests` и `requirements_manager_exporter_tests`
       (matrix/export функционалност), както и GUI binary
       `requirements_manager_gui_tests` (QTEST_MAIN, offscreen QPA платформа).
- [x] 3. Тестовете се регистрират като CTest targets (`add_test`) и преминават
       на Qt5 и Qt6 builds.

## Проследимост

- **Коммити:** `4825bfe` (feat: requirements manager tests), `a26b603` (feat: add GUI tests for requirements manager), `17ed818` (fix: 53 tests passing on Qt5 and Qt6)
- **Код:** `src/plugins/tests/*`, `tests/plugins/requirements_manager/CMakeLists.txt`, `tests/plugins/requirements_manager/test_main.cpp`, `tests/plugins/requirements_manager/test_graph_widget.cpp`
- **Тестове:** Qt5 + Qt6 builds, 53 теста (requirements manager suite)
