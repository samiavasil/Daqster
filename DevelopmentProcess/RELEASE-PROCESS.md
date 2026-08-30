# Release Process (Процес по издаване на release)

> **Решено (Option A):** директно `develop` → `master` + таг `vX.Y.Z`. **Няма
> отделен release branch.** Този документ описва пълния процес — версиониране,
> release flow, hotfix flow, release checklist, документация и CI роли.

## 1. Версиониране (SemVer)

Проектът следва [Semantic Versioning](https://semver.org/spec/v2.0.0.html) —
формат `MAJOR.MINOR.PATCH` (напр. `0.3.0`, `1.0.0`).

| Компонент | Кога се увеличава |
|-----------|-------------------|
| **MAJOR** | Breaking промени (несъвместими промени в API/поведение) |
| **MINOR** | Нови функционалности (backward-compatible) |
| **PATCH** | Бъгфиксове (backward-compatible) |

- **0.x (начален development):** breaking промени са **позволени в MINOR**
  (напр. `0.2.0` → `0.3.0` може да съдържа breaking промени). MAJOR остава `0`,
  докато проектът не е стабилен.
- **Pre-release:** `-rc.1`, `-rc.2`, ... (release candidate) — за версии преди
  финалния release.

### Източник на версията

- **`VERSION` файл** (в корена на repo-то) е единственият източник на истината.
- **`scripts/version.sh set X.Y.Z`** обновява **всичките места**, които носят
  версията:
  - `VERSION` файла
  - plugin `CMakeLists.txt` × 3 (`demo_nodeditor_nodes`, `node_editor_ide`, `requirements_manager`)
  - plugin `*Interface.json` × 8 (`"Version": ...` — вкл. legacy QtCoinTrader + тестовите)
  - plugin docs `README.md` × 2 (`PLUGIN_VERSION = "..."`)
  - `CHANGELOG.md` + `CHANGELOG.en.md` (добавя секция `## [X.Y.Z] - дата` под `## [Unreleased]`)
  - Root `CMakeLists.txt` **не се пипа** — той чете версията от `VERSION` файла
    при configure; `*Interface.cpp` файловете **не се пипат** — те ползват
    `DAQSTER_PLUGIN_VERSION` compile definition-а от `create_plugin()`.
- Скриптът е **идемпотентен** — местата, които вече са на целевата версия, не се пипат.
- **`scripts/version.sh get`** — отпечатва текущата версия от `VERSION`.
- **`scripts/version.sh check`** — проверява дали всички места съвпадат с `VERSION`
  (exit 1 при drift).
- **`scripts/suggest_version.sh`** — release-time semver bump check: чете текущата
  версия от `VERSION`, инспектира git историята от последния `v*` tag и предлага
  следващата версия (breaking → major, feat → minor, fix → patch). Флагове:
  `--dry-run` (само proposal), `--yes` (прилага без prompt).
- **CI drift check:** `version-sync.yml` проверява при всеки push/PR към
  `develop`/`master` дали всички места съвпадат с `VERSION` — **fail-ва** при
  несъответствие (съобщението казва да се пусне `./scripts/version.sh set X.Y.Z`).

## 2. Release flow (Option A — директно в master)

```
feature branches → develop (PR, CI проверки)
                        │
                        ▼  (когато сме готови)
             1. Feature freeze (само бъгфиксове)
             2. Stabilization + финална верификация
             3. Version bump: ./scripts/suggest_version.sh
                → преглед на proposal-а → ./scripts/version.sh set X.Y.Z
                + changelog [Unreleased] → [X.Y.Z]
             4. Merge develop → master
             5. Push таг vX.Y.Z (annotated)
             6. release.yml автоматично: build Qt6 (Linux+Windows)
                + тестове + smoke + AppImage/tarball/ZIP
                + SHA256SUMS + GitHub Release (body от changelog)
```

### Стъпките подробно

1. **Feature freeze** — спират се нови функционалности; в `develop` влизат само бъгфиксове.
2. **Stabilization + финална верификация** — всички CI проверки зелени, release checklist-ът (долу) е изпълнен.
3. **Version bump** — `./scripts/suggest_version.sh` предлага следващата semver версия
   (breaking → major, feat → minor, fix → patch) с обосновка от git историята;
   след преглед `./scripts/version.sh set X.Y.Z` обновява всички места; changelog-ът
   получава секция `## [X.Y.Z] - дата` (Unreleased записите стават новия release,
   свеж `[Unreleased]` остава отгоре).
4. **Merge `develop` → `master`** — `master` вече е released state.
5. **Push таг `vX.Y.Z`** — **annotated** таг (`git tag -a vX.Y.Z -m "..."`).
6. **`release.yml`** се задейства автоматично от тага и прави: build Qt6 (Linux + Windows), тестове, smoke, пакетиране (AppImage + tarball за Linux, ZIP за Windows), `SHA256SUMS` и създава **GitHub Release** с body от съответната changelog секция. Първата стъпка на workflow-а проверява дали тагът съвпада с `VERSION` файла (fail-ва при несъответствие).

## 3. Hotfix flow (критичен бъг в пуснат release)

```
vX.Y.Z tag → hotfix/X.Y.(Z+1) → fix → merge master + tag vX.Y.(Z+1) → merge develop
```

1. Бранч от тага: `git checkout -b hotfix/X.Y.(Z+1) vX.Y.Z`
2. Фикс + тестове + CI проверки.
3. Merge в `master` + таг `vX.Y.(Z+1)` (annotated) → `release.yml` публикува hotfix release-а.
4. Merge обратно в `develop`, за да не се загуби фиксът.

## 4. Release checklist (задължителна верификация)

Преди тагване на `vX.Y.Z`:

- [ ] Qt6 builds PASS (Linux + Windows, CI)
- [ ] ctest green (всички test binaries)
- [ ] Smoke тестове на пакетираните бинарки (AppImage/ZIP)
- [ ] `./scripts/suggest_version.sh --dry-run` — proposal-ът е прегледан
- [ ] version-sync check PASS (VERSION = X.Y.Z навсякъде)
- [ ] Changelog BG + EN обновени с дата
- [ ] Таг vX.Y.Z на master (annotated)
- [ ] GitHub Release с артефакти + SHA256SUMS

## 5. Документацията

- **`master` = released state** → GitHub Pages билдва от `master` = документацията
  на release-а (автоматично синхронизирана).
- Docs промени влизат през `develop` → `master` (като кода).
- **Няма отделна release документация** (Option A) — документацията на `master`
  винаги отговаря на последния release.

## 6. CI роли

| Workflow | Роля |
|----------|------|
| `ci.yml` | PR/push проверки — Qt6 primary (Linux + Windows), Qt5 compat (Linux), тестове, smoke |
| `release.yml` | Tag-triggered release pipeline (`v*.*.*`) — build, тестове, smoke, пакетиране, checksums, GitHub Release |
| `version-sync.yml` | Drift check — версията съвпада навсякъде |
| `pages.yml` | **Изключен** (`pages.yml.disabled`) — сайтът ползва UI-based deployment (Settings → Pages → Deploy from a branch: `master/docs`), workflow не е нужен; опционален, re-enableable |
| `render-plantuml.yml` | Рендерира PlantUML диаграмите (`docs/diagrams/*.puml` → SVG) |
| DeepSource | **Изключен** (`deepsource.yml.disabled`) — опционален, re-enableable |

## 7. Кой прави какво

| Дейност | Кой |
|---------|-----|
| Разработка | feature branches → `develop` (PR) |
| Release | **Човекът (потребителят)** — version bump, merge, таг; **CI** — build/тестове/публикуване |
| Hotfix | **Човекът** — hotfix бранч, merge, таг |