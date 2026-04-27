[Български](./BuildHelpers.md) | [English](./BuildHelpers.en.md)

# Build Helper Scripts

Скриптовете в `tools/build_helpers/` са помощни инструменти за разработка, тестване на build конфигурации и управление на external submodule-и.

## Общ преглед

| Скрипт | Цел |
|---|---|
| `manage_upstream.sh` | Управление на upstream remote за external libs |
| `debug_control.sh` | Контрол на ptrace за GDB attach |
| `test_qt_versions.sh` | Автоматичен тест на Qt5/Qt6 detection |
| `test_qt6_qtrest.sh` | Тест на Qt6 build с QtRest |

---

## manage_upstream.sh

Управлява upstream remote за external библиотеките (`nodeeditor` и `qtrest_lib`), позволява fetch, merge и синхронизация с upstream проектите.

**Употреба:**
```bash
./tools/build_helpers/manage_upstream.sh [--dry-run] [--yes] <command>
```

**Команди:**

| Команда | Описание |
|---|---|
| `status` | Показва текущо upstream HEAD и дали са налични нови commit-и |
| `fetch` | Изтегля нови commit-и от upstream remote |
| `check` | Проверява колко нови commit-а има спрямо upstream |
| `setup` | Добавя `upstream` remote ако липсва |
| `merge <lib\|all>` | Merge upstream промени в submodule; `all` — и двете libs |
| `cherry-pick <commit>` | Cherry-pick на конкретен commit от upstream |
| `help` | Показва help |

**Опции:**

| Опция | Описание |
|---|---|
| `--dry-run` | Показва какво би направил без да изпълнява |
| `--yes` | Не пита за потвърждение |

**Env override:**
```bash
NODEEDITOR_UPSTREAM=https://github.com/my-fork/nodeeditor.git \
  ./tools/build_helpers/manage_upstream.sh fetch
```

**Пример:**
```bash
# Проверка за нови commit-и
./tools/build_helpers/manage_upstream.sh check

# Merge на nodeeditor от upstream
./tools/build_helpers/manage_upstream.sh merge nodeeditor

# Dry-run преди merge
./tools/build_helpers/manage_upstream.sh --dry-run merge all
```

---

## debug_control.sh

Контролира `kernel.yama.ptrace_scope` за разрешаване на GDB remote attach към процеси (нужно за debugging на plugins в Daqster).

> **⚠️ Изисква `sudo`.** Промяната е временна (до рестарт) освен ако не е записана в `/etc/sysctl.d/`.

**Употреба:**
```bash
./tools/build_helpers/debug_control.sh <команда> [--yes]
```

**Команди:**

| Команда | Описание |
|---|---|
| `on` / `enable` | Задава `ptrace_scope=0` (разрешава attach) |
| `off` / `disable` | Задава `ptrace_scope=1` (ограничава attach) |
| `status` | Показва текущата стойност |
| `restore` | Възстановява стойността отпреди последната промяна |
| `help` | Показва help |

**Пример за debug сесия:**
```bash
# Преди debugging
./tools/build_helpers/debug_control.sh on

# ... GDB attach, debug ...

# След debugging
./tools/build_helpers/debug_control.sh restore
```

---

## test_qt_versions.sh

Тества чрез cmake configure дали Qt версия detection работи правилно за всички сценарии: USE_QT6 flag и auto-detect от CMAKE_PREFIX_PATH.

**Употреба:**
```bash
./tools/build_helpers/test_qt_versions.sh
```

**ENV опции:**

| Променлива | Default | Описание |
|---|---|---|
| `BUILD_DIR` | `test_build` | Директория за тестовия build |
| `QT5_PREFIX` | `/mnt/Builder/bin/Linux/Qt/5.15.2/gcc_64` | Path към Qt5 |
| `QT6_PREFIX` | `/mnt/Builder/bin/Linux/Qt/6.9.2/gcc_64` | Path към Qt6 |

**Какво тества:**
1. Qt5 с `USE_QT6=OFF` + Qt5 prefix
2. Qt6 с `USE_QT6=ON` + Qt6 prefix
3. Auto-detect с Qt5 prefix (без USE_QT6)
4. Auto-detect с Qt6 prefix (без USE_QT6)

**Пример:**
```bash
QT5_PREFIX=/usr/lib/qt5 ./tools/build_helpers/test_qt_versions.sh
```

---

## test_qt6_qtrest.sh

Тества Qt6 build и проверява дали `qtrest_lib` артефактите са извадени. Полезен след промени в QtRest за Qt6 compatibility.

> **⚠️ Статус:** QtRest все още има Qt6 compatibility issues. Скриптът е за проверка на прогреса.

**Употреба:**
```bash
./tools/build_helpers/test_qt6_qtrest.sh [--clean] [--qt-prefix PATH] [--build-dir DIR]
```

**Опции:**

| Опция | Default | Описание |
|---|---|---|
| `--clean` | off | Изтрива build директорията преди configure |
| `--qt-prefix PATH` | `/mnt/Builder/bin/Linux/Qt/6.9.2/gcc_64` | Path към Qt6 |
| `--build-dir DIR` | `build_qt6_qtrest` | Директория за build-а |

**Пример:**
```bash
# Чист Qt6 build тест
./tools/build_helpers/test_qt6_qtrest.sh --clean

# С друг Qt6 path
./tools/build_helpers/test_qt6_qtrest.sh --qt-prefix /usr/lib/qt6
```
