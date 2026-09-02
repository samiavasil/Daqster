# REQ-SW-PL-038: DAQSTER_AUTOSTART_FLOW — Headless .flow Scene Load at Startup

- **Статус:** ACTIVE
- **Приоритет:** High
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-02
- **Родител:** REQ-SW-PL-014
- **Зависи от:** REQ-SW-PL-037

## Описание

NodeEditor IDE-то трябва да поддържа headless зареждане на `.flow` сцена при
старт чрез environment variable `DAQSTER_AUTOSTART_FLOW=<path>` — за
автоматизирано perf/памет тестване без GUI интеракция:

1. **Autostart flow load:** Ако `DAQSTER_AUTOSTART_FLOW` е зададен, IDE-то
   зарежда `.flow` сцената от подадения път веднага след старт (без файлов
   диалог). Зареждането минава по tolerant-пътя от REQ-SW-PL-037 —
   нерегистрирани нод типове се пропускат вместо crash.
2. **Video autostart върху заредена сцена:** Ако `DAQSTER_VIDEO_FILE` е
   зададен, върху заредената сцена се изпълнява същата логика като
   `autoStartVideo()` — `VideoFileSource` се конфигурира с файла, бутонът
   "Play" се натиска и "Perf" checkbox-ът на `VideoOutput` се включва.
   Нодовете се намират по model-name в заредения граф (не по id-та, създадени
   от програмния builder).
3. **Рефакторинг:** Логиката "configure source + Play + Perf" се изнася в
   преизползваем helper `startVideoPlayback()`, който се вика и от
   `autoStartVideo()`, и от flow autostart пътя.

## Acceptance Criteria

- [ ] 1. `DAQSTER_AUTOSTART_FLOW=<path>` зарежда `.flow` сцената при старт.
- [ ] 2. Нерегистрирани нодове се пропускат (tolerant path, REQ-SW-PL-037).
- [ ] 3. Ако `DAQSTER_VIDEO_FILE` е зададен, `VideoFileSource` се конфигурира
       + Play + Perf.
- [ ] 4. Qt5 + Qt6 builds PASS.
- [ ] 5. Без crash на malformed/missing-node flow-ве.

## Проследимост

- **Коммити:** (ще се попълни)
- **Код:** `src/plugins/node_editor_ide/NodeEditorIdeObject.h/.cpp`
- **Документация:** `CHANGELOG.md`, `DevelopmentProcess/TEST-STRATEGY.md`
- **Тестове:** Qt5 + Qt6 builds + headless smoke на 8-те `.flow` файла в
  `tests/data/` (video_graph, multi_effect_video, number_graph, audio_graph,
  llm_graph, empty_scene, missing_node, malformed)