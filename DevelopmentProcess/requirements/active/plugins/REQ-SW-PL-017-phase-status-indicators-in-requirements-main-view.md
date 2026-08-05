# REQ-SW-PL-017: Phase Status Indicators in the Requirements Main View

- **Статус:** ACTIVE
- **Приоритет:** P2
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-06
- **Родител:** REQ-SW-PL-001
- **Зависи от:** REQ-SW-PL-002

## Описание

Главният изглед (първи прозорец) на Requirements Manager — списъкът/дървото и
preview панелът — показва за всяко изискване само статус, приоритет и
acceptance-criteria чекбоксове. Фазовата прогресия (архитектура/дизайн →
имплементация → тестване) не се вижда: едно имплементирано, но нетествано
изискване изглежда идентично на ненапочнато такова в главния изглед (разликата
се вижда едва в Traceability Matrix, който е отделен таб).

Това изискване добавя **фазов индикатор** в preview панела на главния изглед,
изведен от проследимост полетата на изискването:

- Архитектура/Дизайн ← `Документация:`
- Имплементация ← `Код:` (заедно с `Коммити:`)
- Тестване ← `Тестове:`

Семантика: **празно/„—" поле = ✗** (не е записано/направено), **непразно поле = ✓**
(записано). Индикаторът е **информативен** — „✓" означава *записано* в
проследимостта, **не** *верифицирано* (непразно ≠ проверено/работещо). Статусът
остава грубия gate: `DONE` продължава да изисква пълна верификация (Qt5/Qt6
builds + unit тестове + headless smoke test) по RDD-PROCESS. Индикаторите не
променят статус, валидатор или lifecycle логика.

Обхват: само главният изглед (preview). НЕ се добавя фазова колона в
дървото/списъка и НЕ се добавя колона в Traceability Matrix/експортерите —
пълната traceability-chain работа е извън обхвата на това изискване.

## Acceptance Criteria

- [ ] 1. **Parser: `Документация:` → `Requirement::docs`.** `Requirement`
      (`RequirementsParser.h:35-71`) получава член `QString docs` (след `tests`,
      ~ред 49). `RequirementsParser::parseFile()` (`RequirementsParser.cpp:409-515`)
      парсва `- **Документация:**` под `## Проследимост` в `out.docs` — огледално
      на Коммити/Код/Тестове (ред 488-493) — като редът остава и в raw
      `traceability` текста (fall-through contract, `handled` остава `false`).
- [ ] 2. **Phase computation (QtCore-only, unit-testable).** В `RequirementsParser.h`
      се добавя `struct PhaseStatus { bool architecture, implementation, testing; };`
      и `PhaseStatus phaseStatus(const Requirement&)` — `architecture` ← `docs`,
      `implementation` ← `code`, `testing` ← `tests`. Непразно след trim поле =
      `true`; празно, whitespace или „—" = `false`.
- [ ] 3. **Preview rendering.** `RequirementsWidget::updatePreviewText()`
      (`RequirementsWidget.cpp:676-745`) показва в главния preview панел фазова
      линия „Архитектура ✓/✗ · Имплементация ✓/✗ · Тестове ✓/✗", изведена от
      `phaseStatus(req)` (вмъкване след Status/Priority реда, ~ред 683).
      Не-ASCII литералите се пишат задължително през `QStringLiteral`.
- [ ] 4. **Status interplay (informative only).** Статусът не се променя: `DONE`
      продължава да изисква пълна верификация по RDD-PROCESS; `phaseStatus` не се
      използва от валидатора, от matrix-а или от lifecycle логиката — само от
      preview.
- [ ] 5. **Tests.** (a) `TestParser::parseDirectory_traceabilityFields()`
      (`test_parser.cpp:254-323`) се разширява с `- **Документация:**` ред във
      фикстурата и `QCOMPARE(req->docs, ...)`; (b) нови TestParser слотове за
      `phaseStatus()`: всички празни → 0/3 `true`, всички попълнени → 3/3,
      частично (само `code`) → 1/3, „—"/whitespace → `false`. Qt5 + Qt6 builds;
      съществуващата suite остава зелена (68/68).
- [ ] 6. **Docs.** `docs/Architecture/plugins/README.md` (Requirements Manager
      секция) документира phase-индикаторите и тяхната семантика
      (записано ≠ верифицирано).

## Проследимост

- **Коммити:** *(попълва се при комит)*
- **Код:** `src/plugins/requirements_manager/RequirementsParser.{h,cpp}` (нов `Requirement::docs` + парсване на `Документация:` + `phaseStatus()`), `src/plugins/requirements_manager/RequirementsWidget.cpp` (phase линия в `updatePreviewText()`)
- **Документация:** `docs/Architecture/plugins/README.md` (Requirements Manager section)
- **Тестове:** Qt5 + Qt6 builds; `tests/plugins/requirements_manager/test_parser.{h,cpp}` (разширен `parseDirectory_traceabilityFields` + нови `phaseStatus` слотове) в shared binary `requirements_manager_tests`; 68/68 PASS на Qt5/Qt6

## Бележки по имплементацията (план)

- Хелперът `phaseStatus()` трябва да е **QtCore-only** и да живее в
  `RequirementsParser.h`, за да е unit-testable в headless-а
  `requirements_manager_tests` (GUI файлове не се компилират в този binary —
  виж `tests/plugins/requirements_manager/CMakeLists.txt:30-62`).
  Контрапример: `RequirementsWidget::filterRequirementsByRepo` е static „за
  unit-testability", но няма unit тест точно защото е в GUI файл.
- Fall-through contract: редът `- **Документация:**` трябва да остане и в
  `traceability` текста (текущият тест проверява `traceability.contains(...)`).
- Семантика „—": PL-001 има `- **Коммити:** —`; за `docs`/`code`/`tests` празно,
  whitespace или „—" = ✗ (не е записано). `Коммити:` не е отделен индикатор.
- Известно следствие (честно): PL-013/014/015 са DONE, но нямат `Документация:`
  ред → ще покажат „Архитектура ✗" в главния изглед. Това е коректно според
  семантиката „записано ≠ верифицирано"; backfill-ът е извън обхват.
