# REQ-SW-PL-052: Пакетиране за дистрибуция (Flatpak/AppImage)

- **Статус:** ACTIVE
- **Приоритет:** Medium
- **Отговорник (роля):** Ivan (Implementation)
- **Дата:** 2026-09-07
- **Родител:** —
- **Зависи от:** —

## Описание

Решава проблема с Qt версиите (Ubuntu 24.04 има Qt 6.4.2, проектът изисква
6.8.3). Flatpak (SDRangel-ски манифест, капсулира Qt и OpenSSL) или AppImage.
Цел: потребителите да не инсталират ръчно Qt.

## Acceptance Criteria

- [ ] 1. AppImage или Flatpak build конфигурация
- [ ] 2. Работи на Ubuntu 24.04 без ръчна Qt инсталация
- [ ] 3. Плъгините се включват в пакета
- [ ] 4. Документация за build/packaging
- [ ] 5. (Опционално) CI pipeline
- [ ] 6. Тестове (отложени по текущата инструкция)

## Проследимост

- **Коммити:** (pending commit)
- **Код:** `tools/` (create_appimage.sh, Flatpak manifest), `.github/workflows/`
  (CI pipeline), `docs/operations/`

## Бележка

Изискването е създадено по решение на потребителя (2026-09-07) след
проучването на SDRangel (Flatpak манифест, капсулира Qt и OpenSSL) и
констатирания Qt version проблем (Ubuntu 24.04: Qt 6.4.2 vs изискване 6.8.3).
Съществуващата AppImage инфраструктура (REQ-SW-PL-036) е отправната точка.
Архитектурното предложение е в `docs/Architecture/runtime-mode-architecture.md`
(секция 4, Фаза 3). Unit тестове са ОТЛОЖЕНИ по стоящата инструкция
„НОВИ ТЕСТОВЕ СТОП".