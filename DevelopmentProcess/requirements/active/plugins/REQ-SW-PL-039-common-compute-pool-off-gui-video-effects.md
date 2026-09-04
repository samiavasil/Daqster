# REQ-SW-PL-039: Common compute thread pool + off-GUI video effect processing

- **Статус:** ACTIVE
- **Приоритет:** High
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-04
- **Родител:** REQ-SW-PL-028
- **Зависи от:** REQ-SW-PL-032, REQ-SW-PL-023

## Описание

1. **Common/shared thread pool.** Нов `ComputePool` singleton в
   `src/plugins/common/Threading/` (споделен между всички нодове, не per-node):
   per-key latest-wins submit (frame skipping), per-key сериализация (запазва
   ring-buffer контракта на DaqDisplayNode), `cancel(key)` за безопасно
   унищожаване на нод, метрики (submitted/started/completed/skipped + fps).
2. **DaqDisplayNode мигрира** от node-owned `QThreadPool` (maxThreadCount=1)
   към общия пул (per-key сериализация).
3. **VideoEffectNode CPU path** се мести на пула: `setInData` (GUI thread)
   snapshot-ва frame + params и submit-ва; worker-ът конвертира frame-а сам
   (без споделения `asImage()` кеш), прилага ефекта и post-ва резултата с
   `Qt::QueuedConnection`; GUI thread само обвива резултата и емитира
   `dataUpdated(0)`.
4. **Frame skipping:** ако worker-ът изостава, новите кадри се заменят/пускат
   (process LATEST only) — без растяща опашка.
5. **Метрика:** skipped/total frames + реален fps след ефектите — label в
   widget-а + опционален `[PERF] effect` console ред.

## Acceptance Criteria

- [ ] 1. `ComputePool` singleton съществува в `src/plugins/common/Threading/`.
- [ ] 2. `submitLatest(key, task)` — latest-wins: queued се заменя, running →
       pending; per-key сериализация.
- [ ] 3. `cancel(key)` премахва queued задачи и чака running (≤ timeout) —
       безопасно унищожаване на нод.
- [ ] 4. DaqDisplayNode ползва общия пул; ring buffer-ът остава worker-only.
- [ ] 5. VideoEffectNode CPU ефекти никога не вървят на GUI thread.
- [ ] 6. Frame skipping: при натоварване изходът е подредица на входа
       (latest-wins), без растяща опашка.
- [ ] 7. Метриката показва total/processed/skipped + реален fps.
- [ ] 8. `asImage()` не се вика от worker; worker-ът конвертира свой copy.
- [ ] 9. Qt5 + Qt6 builds PASS.
- [ ] 10. Unit тестове: `test_compute_pool` + `test_video_effect_node` green.

## Проследимост

- **Код:** `src/plugins/common/Threading/ComputePool.{h,cpp}`,
  `src/plugins/demo_nodeditor_nodes/Displays/DaqDisplay/DaqDisplayNode.{h,cpp}`,
  `src/plugins/demo_nodeditor_nodes/Sources/Video/VideoEffectNode.{h,cpp}`,
  `src/plugins/common/NodeDataTypes/VideoFrameData.h`
- **Тестове:** `tests/plugins/demo_nodeditor_nodes/test_compute_pool.*`,
  `test_video_effect_node.*`