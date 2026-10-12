# Release & версиониране

Родител: [Operations](README.md) | [Индекс](../index.md)

Пълен процес: [`DevelopmentProcess/RELEASE-PROCESS.md`](../../DevelopmentProcess/RELEASE-PROCESS.md)
(това е каноничният документ). Тук е кратката версия за разработчици.

## Един източник на истина

**`VERSION` файл** (в корена) е единственият източник на версията. Root
`CMakeLists.txt` я чете при configure; `*Interface.cpp` файловете ползват
`DAQSTER_PLUGIN_VERSION` compile definition-а от `create_plugin()` — те **не** се
пипат ръчно.

## Инструменти (`scripts/`)

| Команда | Действие |
|---------|----------|
| `./scripts/version.sh get` | Отпечатва текущата версия |
| `./scripts/version.sh set X.Y.Z` | Обновява **всички** места (идемпотентно) |
| `./scripts/version.sh check` | Проверява, че всички места съвпадат с `VERSION` (exit 1 при drift) |
| `./scripts/suggest_version.sh` | Предлага следващата semver версия от git историята |
| `./scripts/suggest_version.sh --dry-run` | Само proposal, без прилагане |
| `./scripts/suggest_version.sh --yes` | Прилага без prompt |

`suggest_version.sh` чете текущата версия от `VERSION`, инспектира историята от
**последния `v*` tag, който съответства на `VERSION`** (не `git describe` — release
таговете са на `master` и не са ancestors на `develop`), класифицира комитите
(`feat` → minor, `fix` → patch, breaking → major) и предлага следващата версия с
обосновка. `version.sh set` обновява: `VERSION`, plugin `CMakeLists.txt` × 3, plugin
`*Interface.json` × 8, plugin docs `README.md` × 2, `CHANGELOG.md` + `CHANGELOG.en.md`.

## SemVer правила

| Компонент | Кога се увеличава |
|-----------|-------------------|
| **MAJOR** | Breaking промени |
| **MINOR** | Нови функционалности (backward-compatible) |
| **PATCH** | Бъгфиксове |

> **0.x (текущо състояние):** breaking промени са **позволени в MINOR** — MAJOR
> остава `0`, докато проектът не е стабилен. Затова при 0.x breaking промяна дава
> MINOR bump (напр. `0.3.2` → `0.4.0`), **не** `1.0.0`.

## Release flow (накратко)

```
feature branches → develop (PR + CI)
   → feature freeze → stabilization
   → ./scripts/suggest_version.sh  →  ./scripts/version.sh set X.Y.Z
   → merge develop → master
   → push annotated tag vX.Y.Z
   → release.yml: build (Qt6 Linux+Windows) + тестове + smoke + пакети + GitHub Release
```

**Integration branch е `develop`; `master` е FROZEN** — нищо не се merge-ва в
`master` освен при release (процедура извън проекта). Никога не се branch-ва от
`master`.

## CI

| Workflow | Роля |
|----------|------|
| `ci.yml` | PR/push проверки (Qt6 primary Linux+Windows, Qt5 compat, тестове, smoke) |
| `release.yml` | Tag-triggered release pipeline |
| `version-sync.yml` | Drift check (VERSION = навсякъде) — **fail-ва** при несъответствие |
| `render-plantuml.yml` | `docs/diagrams/*.puml` → SVG |
