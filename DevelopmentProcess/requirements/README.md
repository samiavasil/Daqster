# Requirements — Daqster Framework Tools & Requirements Manager

Система за проследими изисквания за **общите инструменти и компоненти на
frame_work** и приложенията в публичното Daqster repo, както и частните плъгини (напр. `DaqsterAiStudio`).

## Структура

```
DevelopmentProcess/requirements/
├── README.md              # Този файл
├── traceability-matrix.md # Матрица за проследимост (REQ ⟷ Коммит ⟷ Тест)
├── active/                # Активни (незавършени) изисквания
│   ├── plugins/           # REQ-SW-PL-* (requirements manager, node editor, demo plugins)
│   ├── framework/         # REQ-SW-FW-* (frame_work core: plugin manager, discovery, logging, process, shutdown)
│   ├── app/               # REQ-SW-APP-* (application host, apps)
│   └── build/             # REQ-SW-BLD-* (CMake инфраструктура, unit test инфраструктура)
└── archive/               # Завършени/имплементирани изисквания
```

## Именуване

Публичните изисквания (това repo) ползват **типизирана схема** `REQ-SW-<ТИП>-<NN>`
с трицифрен номер (напр. `REQ-SW-PL-001`). Частните изисквания
(DaqsterAiStudio) ползват `REQ-<ПРЕФИКС>-<NN>`.

| Префикс | Област |
|---------|--------|
| `REQ-SW-PL-*` | Plugins (requirements manager, node editor, demo plugins) |
| `REQ-SW-FW-*` | Framework (frame_work core: plugin manager, discovery, logging, process, shutdown) |
| `REQ-SW-APP-*` | App (application host, apps) |
| `REQ-SW-BLD-*` | Build & tooling (CMake инфраструктура, unit test инфраструктура) |
| `REQ-PLG-*` / `REQ-AI-*` / `REQ-SEC-*` / `REQ-DOC-*` | Частни (DaqsterAiStudio): plugin framework, AI Studio, сигурност, документация |

> **Superseded ID-та:** старите публични ID-та с префикс `REQ-SW-` без типов
> сегмент (номера 001–024) са заменени от типизираните `REQ-SW-PL-*` /
> `REQ-SW-FW-*` / `REQ-SW-APP-*` / `REQ-SW-BLD-*`. Пълният mapping старо → ново
> е в `RDD-PROCESS.md`.

## Връзки и Проследимост (Relationship Axes)

Всяко изискване съдържа задължителни метаданни за връзки:
- **Родител:** ID на по-общото изискване, от което произтича (йерархия).
- **Зависи от:** ID на изискванията, които трябва да бъдат изпълнени преди или заедно с това (зависимости).

## Многоагентен работен процес

| Роля | Какво прави |
|------|-------------|
| Oscar | Оркестрира, делегира и синтезира |
| Scout | Проучване (research) + планиране |
| Ivan | Имплементация + git |
| Jester | Reality-check / критика |
