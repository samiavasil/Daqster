# REQ-SW-PL-050: Стандартизиран thread lifecycle протокол за нодовете

- **Статус:** ACTIVE
- **Приоритет:** High
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-07
- **Родител:** REQ-SW-PL-014
- **Зависи от:** —

## Описание

NodeDelegateModel получава виртуален stop()/wait() протокол. Всички нодове с
фонови нишки го имплементират. deleteNode() и ShutdownHandler веригата викат
stop() преди унищожаване. Цел: чисто спиране без crash при затваряне (доказан
проблем — headless/Xvfb крашове). Урок от SDRangel: stop → wait → unload.

Нодовете с фонови нишки (QThread): PlutoSdr, Pcap, AudioSource, VideoEffect.
Нодовете с QTimer: Gamepad, SystemMonitor, GpuMonitor, JackDetect. LLama ползва
QProcess.

## Acceptance Criteria

- [ ] 1. NodeDelegateModel има virtual stop() (и wait() където е приложимо)
- [ ] 2. Нодовете с фонови нишки имплементират stop(): PlutoSdr, Pcap,
       AudioSource, VideoEffect, LLama
- [ ] 3. QTimer-базираните нодове също: Gamepad, SystemMonitor, GpuMonitor,
       JackDetect
- [ ] 4. deleteNode() вика stop() преди унищожаване на модела
- [ ] 5. ShutdownHandler веригата гарантира stop() → wait() преди exit
- [ ] 6. Няма crash при затваряне с активни нишки (проверка headless/Xvfb)
- [ ] 7. Тестове (отложени по текущата инструкция)

## Проследимост

- **Коммити:** (pending commit)
- **Код:** `src/plugins/common/capabilities/INodeProvider.h` (NodeDelegateModel
  base), `src/plugins/demo_nodeditor_nodes/Sources/` (PlutoSdr, Pcap,
  AudioSource, VideoEffect, Gamepad, SystemMonitor, GpuMonitor, JackDetect),
  `src/plugins/node_editor_ide/` (deleteNode), `src/frame_work/base/src/platform/`
  (ShutdownHandler)

## Бележка

Изискването е създадено по решение на потребителя (2026-09-07) след
проучването на SDRangel (stop → wait → unload) и одита на текущото състояние:
per-model destructor cleanup, НЯМА unified stop()/wait() на NodeDelegateModel;
ShutdownHandler → quit(); async deleteLater верига; нишките може да работят при
exit. Архитектурното предложение е в `docs/Architecture/runtime-mode-architecture.md`
(секция 4.3). Unit тестове са ОТЛОЖЕНИ по стоящата инструкция „НОВИ ТЕСТОВЕ СТОП".