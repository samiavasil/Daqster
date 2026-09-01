# REQ-SW-PL-026: VideoFileSource Stop + Seek Controls

- **Статус:** DONE
- **Приоритет:** Medium
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-11
- **Родител:** REQ-SW-PL-018
- **Зависи от:** REQ-SW-PL-018

## Описание

`VideoFileSourceNode` (Sources/Video) възпроизвежда локално видео, но до този
момент има само Play/Pause — няма как да се спре възпроизвеждането или да се
превърти позицията ръчно. Това изискване добавя транспортен ред (transport
controls) в embedded widget-а на node-а: **Stop**, **Seek назад (`<< -5s`)**, **Seek
напред (`>> +5s`)** и **time label** (`позиция / дюрация`). Същевременно коригира
reset-а на `m_loadedPath` при Stop/EndOfMedia, така че следващото play да започва
от началото на файла.

## Acceptance Criteria

- [x] 1. **Stop control.** `m_stopButton` (tr("Stop")) в seek реда на widget-а
       (`VideoFileSourceNode.cpp:212-224`); disabled по подразбиране, enabled при
       Play (`:271-274`). `onStopClicked()` (`:277-287`) вика `m_player->stop()`,
       изчиства `m_loadedPath`, сваля `m_isPlaying = false`, `updatePlayButton()` →
       "Play", статус "Stopped" (gray) и disabled-ва Stop + двата Seek бутона.
- [x] 2. **Seek назад -5s.** `m_seekBackButton` (tr("<< -5s"));
       `onSeekBackClicked()` (`:289-293`) сваля позицията с 5000 ms и я clamp-ва
       долу на `qMax(qint64(0), pos - 5000)` — не слиза под 0:00.
- [x] 3. **Seek напред +5s.** `m_seekForwardButton` (tr(">> +5s"));
       `onSeekForwardClicked()` (`:295-301`) вдига позицията с 5000 ms и я clamp-ва
       горе на `qMin(dur, pos + 5000)` — не надхвърля дюрацията (guard `dur > 0`).
- [x] 4. **Time label.** `m_timeLabel` (tr("0:00 / 0:00")) показва `MM:SS / MM:SS`
       (`позиция / дюрация`); обновява се в `positionChanged` lambda
       (`:72-79`) с 2-цифрен zero-padded формат за минути и секунди.
- [x] 5. **Жизнен цикъл + reset на m_loadedPath.** При `EndOfMedia`
       (`onMediaStatusChanged`, `:390-399`) — stop, `m_loadedPath.clear()` (force
       media re-set на следващото play) и disabled на Stop + Seek. На
       `onPlayPauseClicked` (`:250-275`) бутоните се enabled-ват само след успешно
       play.

## Проследимост

- **Коммити:** `df4a308` (feat (REQ-SW-PL-026): VideoFileSource — Stop + Seek
  controls (+/-5s) + time label) — директно в `develop`
- **Код:** `src/plugins/demo_nodeditor_nodes/Sources/Video/VideoFileSourceNode.{h,cpp}`
- **Тестове:** няма специфични unit тестове за Stop/Seek контролите — покрити от
  Qt5 (5.15.2) + Qt6 (6.9.2) builds + app smoke (без crash); съществуващата test
  suite остава зелена.

## Бележка

Ретроспективно изискване — създадено **след** имплементацията (2026-08-11), която
вече е merge-ната в `develop`. Комит `df4a308` реферира `REQ-SW-PL-026` в
съобщението си, но REQ файлът не беше създаден по онова време; този документ го
запълва. Функционалността е върху `VideoFileSourceNode` от REQ-SW-PL-018
(родител), без промяна на порт layout-а или data flow-а на node-а.
