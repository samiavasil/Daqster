# NodeRunner (двоен режим)

Родител: [apps](README.md) | [Architecture](../README.md)
Виж още: [Core/GUI Split](../core-gui-split.md) · [REST/remote control](../rest-api-remote-control.md) · [Flow формат](../formats/flow-format.md)

`NodeRunner` е **един изпълним файл с два режима**: с видим GUI или напълно
headless. Той зарежда `.flow` сцена и я вдига чрез съответния runtime host.
Източник: `src/apps/NodeRunner/main.cpp`.

## Употреба

```bash
# GUI режим (по подразбиране)
NodeRunner --run scene.flow

# Headless режим (offscreen, без конструиране на widget-и)
NodeRunner --headless --run scene.flow

# Headless + remote control (REST + WebSocket)
NodeRunner --headless --run scene.flow --rest-port 18080 --rest-token <token>
```

| Опция | Смисъл |
|-------|--------|
| `--run <flow.flow>` | Задължителна: `.flow` сцената за зареждане |
| `--headless` | Без видим UI — offscreen platform, никакъв widget не се конструира |
| `--rest-port <port>` | Включва remote-control плоскостта (HTTP/WebSocket) на порта |
| `--rest-bind <addr>` | Адрес за bind (по подразбиране loopback) |
| `--rest-token <token>` | Bearer токен за автентикация на контролната плоскост |
| `--rest-web-root <dir>` | Директория с web конзолата (по подразбиране вградена) |
| `--version` | Версия (от `daqster_version.h`) |

> `--headless` се парсва **преди** app-обектът (който идва от `dlopen`-натия
> launcher), защото platform plugin-ът се чете при конструкция — затова bootstrap-ът
> го чете от суровия `argv`, а не през `QCommandLineParser`.

## Защо е двоен режим

След core/gui split-а (REQ-SW-PL-051) **нито един нод модел не конструира widget на
headless пътя**, затова един и същ `.flow` върви и в двата режима. Остава **един
бинарник** `NodeRunner`, който избира app-обекта по режим чрез малък `dlopen`-нат
launcher (REQ-SW-PL-072): `DaqsterLauncherHeadless` (`QGuiApplication`, без
QtWidgets) при `--headless`, `DaqsterLauncherGui` (`QApplication`) иначе. Така
`NodeRunner` не линква QtWidgets, а GPU/GL оптимизациите работят и в двата режима.

## Споделен AppBootstrap

Runner-специфичната логика в `main.cpp` е минимална — само application type,
име/версия, `--headless`/`--run` опциите и валидацията на пътя. Всичко общо живее в
**`Daqster::AppBootstrap`** (`FrameworkCore`, widget-free, REQ-SW-PL-048):

- `detectHeadless(argc, argv)` + `preparePlatform(...)` — избор на platform plugin.
- `addStandardOptions()` — `--version`, `--rest-*`, ... (общи за всички runner-и).
- `processArguments()`, `installShutdownHandler()`.
- `initializePlugins()` — зарежда плъгините през `QPluginManager`.
- `resolveRuntimeHost(mode)` — намира `IRuntimeHost` за `Headless`/`Gui`.
- `prepareHost(host)` — свързва optional-ната control plane; ако е поискана, но
  неподдържана → **fail loudly**.
- `loadFlow(host, path)` — зарежда сцената.

Това позволява `Daqster` (GUI host) и `NodeRunner` да споделят една bootstrap
логика без дублиране.

## Свързано

- [Daqster (host app)](Daqster.md) — главното GUI приложение
- [ApplicationsManager](ApplicationsManager.md) — стартиране на child приложения
- [Runtime host плъгини](../../plugins/README.md) — `RuntimeHostHeadlessPlugin` / `RuntimeHostGuiPlugin`
