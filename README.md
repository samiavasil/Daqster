# Daqster 
Framework for Qt plugins creation.

Site: https://samiavasil.github.io/Daqster/

Works with Qt5.

## Quick Start (CMake)

### 1) Clone
```
git clone https://github.com/samiavasil/Daqster.git
cd Daqster
git submodule update --init --recursive
```

### 2) Configure & Build
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 3) Run
Бинарните артефакти са в `build/bin/`. Ако е нужно, укажете пътя към динамичните библиотеки и плъгини:
```
cd build/bin
LD_LIBRARY_PATH=./libs:./extlibs/lib ./Daqster
```

## Structure
- `src/frame_work` — ядро за откриване и управление на плъгини
- `src/apps/Daqster` — хост приложение
- `src/plugins` — плъгини (NodeEditor, QtCoinTrader, примери/тестове)
- `src/external_libs` — външни зависимости (nodeeditor, qtrest_lib)

## Notes
- CMake е основният и поддържан билд.
- Предстои добавяне на `install()` цели и пакетиране.

