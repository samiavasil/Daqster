# Daqster 
[Български](./README.md) | [English](./README.en.md)

Daqster е рамка (Qt5) за създаване и зареждане на плъгини, с хост приложение и примерни плъгини.

Сайт: https://samiavasil.github.io/Daqster/

## Бърз старт (CMake)

### 1) Клониране
```
git clone https://github.com/samiavasil/Daqster.git
cd Daqster
git submodule update --init --recursive
```

### 2) Конфигуриране и билд
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 3) Стартиране от build
```
cd build/bin
./Daqster
```

## Инсталация и стартиране
Инсталирай (по подразбиране в `/usr/local` или в избран префикс):
```
cmake --install build --prefix ./install_dir
```
Стартиране:
```
./install_dir/bin/Daqster
```
- Библиотеките се намират автоматично чрез RPATH (`$ORIGIN/../lib`).
- Плъгините се търсят в:
  - `./plugins` (до бинарника),
  - `../lib/daqster/plugins` (инсталационен път),
  - директория от променлива на средата `DAQSTER_PLUGIN_DIR`.

## Структура на проекта
- `src/frame_work` — ядро за търсене/зареждане на плъгини
- `src/apps/Daqster` — хост приложение
- `src/plugins` — плъгини (NodeEditor, QtCoinTrader, примери/тестове)
- `src/external_libs` — външни зависимости (nodeeditor, qtrest_lib)

## Бележки
- Поддържан билд: CMake.
- Пакетиране и разширена документация могат да бъдат добавени.

