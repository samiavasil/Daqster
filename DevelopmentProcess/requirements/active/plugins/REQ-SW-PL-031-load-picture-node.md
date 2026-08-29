# REQ-SW-PL-031: LoadPicture — картинки като видео (QImage → VideoFrameData → GL blit)

- **Статус:** ACTIVE
- **Приоритет:** P2
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-08-24
- **Родител:** REQ-SW-PL-018 (архив: archive/plugins/REQ-SW-PL-018-video-source-and-processing-nodes.md)
- **Зависи от:** REQ-SW-PL-020 (active/plugins/REQ-SW-PL-020-zero-copy-video-frame-display.md)

## Описание

`LoadPicture` зарежда картинка от диск и я пуска по video пайплайна:

1. **`QImage` от диск → `VideoFrameData` → GL blit display** (1 upload).
2. **Един тип носи и видео, и картинки** — LoadPicture произвежда същия
   `VideoFrameData` тип като видео source-ите, с lazy кешове (`asImage()` +
   GL текстура).

## Acceptance Criteria

- [ ] 1. **Типова съвместимост.** `LoadPicture` приема `QImage` (файл/път),
       извежда `VideoFrameData`.
- [ ] 2. **GL blit с 1 upload.** Картинката се показва през GL blit с едно
       upload-ване.
- [ ] 3. **Lazy кешове.** `asImage()` + GL текстура кеш (като всички
       `VideoFrameData`).
- [ ] 4. **Qt5 + Qt6 builds PASS.**
- [ ] 5. **Тестове** (отложени по стояща инструкция).

## Проследимост

- **Коммити:** чака имплементация
- **Код:** чака имплементация
- **Документация:** дизайн документ `video-frame-consolidation-design.md` §3.5;
  статус `2026-08-24-status.md` §12
- **Тестове:** отложени (standing instruction)

## Бележки по имплементацията (план)

- **QImage → VideoFrameData:** зареждане от файл/път, конвертиране към
  `VideoFrameData` с lazy кешове.
- **GL blit:** display с 1 upload (GL текстура кеш).
- **Един тип:** LoadPicture произвежда същия `VideoFrameData` тип като видео
  source-ите — картинките и видеото са взаимозаменяеми в графа.