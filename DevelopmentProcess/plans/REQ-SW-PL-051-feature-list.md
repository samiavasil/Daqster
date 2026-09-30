# REQ-SW-PL-051 — Списък на фичърите в бранча

> ## ⚠️ ИСТОРИЧЕСКИ СНИМЪК — 2026-09-26
>
> Това е checklist-ът от една по-ранна точка на бранча. Оставен е за
> проследимост какво е било проверено тогава, **не като текущо pending work**.
>
> Няколко очаквани резултати по-долу вече не са валидни:
>
> | Очаквано тук | Реално днес |
> |---|---|
> | `build_qt5/bin/libDaqsterCore.so`, `libDaqsterGui.so` | `libFrameworkCore.so`, `libFrameworkGuiPlugin.so` — преименуваното от REQ-SW-PL-053 split (`FrameworkCore` / `FrameworkGuiPlugin`), което вършеше същата работа |
> | `src/core/`, `src/gui/` изтрити, всичко в `src/frame_work/` като 2 библиотеки | `src/core/` и `src/gui/` го няма отдавна; `src/frame_work/base/` е един QtCore-only core, а GUI частта е plugin-ът `FrameworkGuiPlugin` |
> | `Daqster --headless --run` | `NodeRunner --headless --run` (един dual-mode бинарник) |
>
> Актуалното състояние на миграцията е в
> `DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-051-core-gui-separation-headless.md`
> (28 от 30 модела разделени) и в `docs/Architecture/core-gui-split.md`.

- **Бранч:** `feat/REQ-SW-PL-051-core-gui-separation` (base `develop_pre` @ `9dea41f`)
- **Състояние:** working tree чист, 10 commit-а, нищо не е push-вато, нищо не е merge-вато
- **Дата:** 2026-09-26
- **Всички „очаквани" резултати са реално измерени.**

Организирано е по **функционалност**, не по commit. В края е картата
фичър → commit.

---

# Подготовка (направи веднъж)

```bash
cd /mnt/Builder/Projects/samiavasil/daqster

cmake -S . -B build_qt5 -DCMAKE_PREFIX_PATH=/mnt/Builder/bin/Linux/Qt/5.15.2/gcc_64 -DDAQSTER_BUILD_TESTS=ON
cmake --build build_qt5 -j8

cmake -S . -B build_qt6 -DCMAKE_PREFIX_PATH=/mnt/Builder/bin/Linux/Qt/6.9.2/gcc_64 -DDAQSTER_BUILD_TESTS=ON
cmake --build build_qt6 -j8
```

Ако нещо е счупено, **спри тук** — останалите тестове нямат смисъл.

---

# Фичър 1 — Миграция: `src/core/` и `src/gui/` изтрити

**Какво е.** Старите дублирани директории `src/core/` и `src/gui/` са били копия
на `frame_work/`. Изтрити са. Всичко живее под `src/frame_work/` като две
библиотеки — `DaqsterCore` (QtCore + QtNodes) и `DaqsterGui` (QtWidgets, върху
Core).

**Къде да гледаш.** `src/frame_work/CMakeLists.txt` — редове 40–170.

**Тест:**
```bash
for d in src/core src/gui; do
  [ -d "$d" ] && echo "$d: EXISTS (не трябва)" || echo "$d: липсва (правилно)"
done
ls build_qt5/bin/libDaqsterCore.so build_qt5/bin/libDaqsterGui.so
```
**Очаквано:**
```
src/core: липсва (правилно)
src/gui: липсва (правилно)
build_qt5/bin/libDaqsterCore.so
build_qt5/bin/libDaqsterGui.so
```

**Как разбираш, че е изпълнено:** двете библиотеки се линкват и двете съществуват
като `.so`. Ако `libDaqsterCore.so` липсва — миграцията не е завършена.

---

# Фичър 2 — Билдът минава на Qt5 и Qt6

**Какво е.** `c527eba`: поправени include paths, добавени липсващи заглавни
файлове, поправени namespace-и. Преди билдът падаше.

**Тест:**
```bash
cmake --build build_qt5 -j8 2>&1 | grep -ciE "error|warning"
cmake --build build_qt6 -j8 2>&1 | grep -ciE "error|warning"
```
**Очаквано: `0` и `0`.**

Ако имаш warning-и — **не продължавай**, ще ги разгледаме. Всичко в този бранч е
било чисто.

**Как разбираш, че е изпълнено:** 0 грешки **и 0 предупреждения** на двете Qt.

---

# Фичър 3 — Qt и вътрешни библиотеки се линкват PUBLIC

**Какво е.** `link_component_dependencies()` в
`cmake/PluginDependencyManager.cmake` правеше всичко PRIVATE освен `Qt*::`.
Затова вътрешните библиотеки (`DaqsterCore`, `NodeEditorLibrary`, други
plugin-и) не предаваха своя интерфейс нататък — export header-ите не
попадаха на include path и PUBLIC дефиниции се изгубваха.

**Тест А — export header-ът е на include path-а на core plugin-а:**
```bash
f=build_qt5/src/plugins/demo_nodeditor_nodes_core/CMakeFiles/DemoNodeEditorNodesCore.dir/flags.make
grep -o '\-I[^ ]*frame_work[^ ]*' $f | grep -c "build_qt5/src/frame_work"
```
**Очаквано: поне 1** (това е редът с `daqster_core_export.h`).

**Тест Б — PUBLIC дефиницията се пренася:**
```bash
grep -c 'DAQSTER_ENABLE_PERF' $f
```
**Очаквано: поне 1** (`-DDAQSTER_ENABLE_PERF`).

**Тест В — демонстрация на контра (редове 161–167):**
```bash
sed -n '160,168p' cmake/PluginDependencyManager.cmake
```
Трябва да видиш условието:
```cmake
if(LIBRARY MATCHES "^Qt[0-9]+::" OR NOT LIBRARY_IS_IMPORTED)
    target_link_libraries(${COMPONENT_NAME} PUBLIC ${LIBRARY})
else()
    target_link_libraries(${COMPONENT_NAME} PRIVATE ${LIBRARY})
```
Тоест: Qt target-и → PUBLIC, вътрешни (non-imported) → PUBLIC, чужди
импортирани → PRIVATE.

**Как разбираш, че е изпълнено:** тест А и Б минават. Референция: без PUBLIC
линковането билдът пада с
`QPluginInterface.h:23: fatal error: daqster_core_export.h: No such file or directory`.

---

# Фичър 4 — Вградените node модели са преместени в core plugin-а

**Какво е.** `9aa85cc`: `NumberSource`, `NumberDisplay`, `Modulo` и
`ArithmeticLogic` (с нов parser на изрази) вече са **копия в core plugin-а**,
вместо core да зависи от `node_editor_ide`. Те са и фикстурата за headless
тестовете — не искат медиа, мрежа или OpenGL.

**Тест А — моделите са регистрирани в core plugin-а:**
```bash
grep -cE "registerModel<" src/plugins/demo_nodeditor_nodes_core/DemoNodeEditorNodesCoreObject.cpp
```
**Очаквано: `27`**

**Тест Б — първите 4 са новите:**
```bash
grep -oE "registerModel<[A-Za-z0-9_]+>" src/plugins/demo_nodeditor_nodes_core/DemoNodeEditorNodesCoreObject.cpp \
  | head -4
```
**Очаквано:**
```
registerModel<NumberSourceDataModel>
registerModel<NumberDisplayDataModel>
registerModel<ModuloModel>
registerModel<ArithmeticLogicModel>
```

**Тест В — реално изпълнение през тях:**
```bash
cd build_qt5/bin && unset QT_QPA_PLATFORM
./DaqsterHeadless --run ../../tests/data/number_graph.flow --log-console-enabled 1 --log-level Info 2>&1 | grep "loaded nodes"
```
**Очаквано: `loaded nodes= 2 connections= 1`**

> `number_graph.flow` е точно тази фикстура. Ако тук падат unregistered типове,
> Фичър 7 е счупен.

### ⚠️ Известен остатък по този фичър

`9aa85cc` твърди, че е спрял изтичането на `node_editor_ide/BuiltInNodes`
include dirs в core plugin-а. **Това не е вярно.** `DaqsterCore` ги излага
`PUBLIC` (`src/frame_work/CMakeLists.txt:138-147`), а Фичър 3 разпространява
това нататък.

Проверка:
```bash
f=build_qt5/src/plugins/demo_nodeditor_nodes_core/CMakeFiles/DemoNodeEditorNodesCore.dir/flags.make
grep -o '\-I[^ ]*node_editor_ide/BuiltInNodes[^ ]*' $f | sort -u | wc -l
```
**Очаквано (лошо): `10`**

Реално само **1** е необходим — `BuiltInNodes/Library/connectors`, за
двата connector TU-та, които `1e528c9` компилира направо. Останалите 9 са
мъртви. Кодът на модела вече не ги ползва (файловете са преместени вътре в
core plugin-а).

**Как разбираш, че е изпълнено:** тест В минава. Остатъкът от 10-те include-а
е козметичен, но е реално замърсяване на core/gui разделението.

---

# Фичър 5 — Plugin discovery не зависи вече от `.ini` файл

**Какво е (бъг).** `SearchForPlugins()` подаваше на `discoverPlugins()` списък
с plugin-и, описан в `~/.config/.../daqster_qt5.ini`. Този файл преживява
рестартите. Така че **от втория рън нататък всеки plugin съвпадаше със
записан hash → „вече известен" → пропускан**. Нищо не се зареждаше и
**нямаше нито един ред в лога**.

**Тест — стъпка 1 (изтрий persistence-а):**
```bash
rm -f ~/.config/DaqsterHeadless/daqster_qt5.ini ~/.config/Daqster/daqster_qt5.ini
cd build_qt5/bin && unset QT_QPA_PLATFORM
./DaqsterHeadless --run ../../tests/data/number_graph.flow --log-console-enabled 1 --log-level Info 2>&1 | grep "loaded nodes"
```
**Очаквано: `loaded nodes= 2 connections= 1`**

**Тест — стъпка 2 (повтори, БЕЗ да триеш нищо):**
```bash
./DaqsterHeadless --run ../../tests/data/number_graph.flow --log-console-enabled 1 --log-level Info 2>&1 | grep "loaded nodes"
```
**Очаквано: абсолютно същото.**

> **Най-важният тест в целия бранч.** Ако стъпка 2 е празна или каже
> „skipping" — този бъг се е върнал.

**Тест — за GUI:**
```bash
./Daqster --log-console-enabled 1 --log-level Debug 2>&1 | grep -c "PLUGIN METADATA"
```
**Очаквано: `5`**

> Логът на GUI излиза **само на `Debug`**. На `Info` няма изход — не е бъг.

**Как разбираш, че е изпълнено:** стъпка 1 и 2 дават едно и също, GUI намира 5
plugin-а.

---

# Фичър 6 — Core plugin-ът може да бъде зареден (`dlopen`)

**Какво е (бъг).** Plugin-ът излизаше с **31 недефинирани символа**.
`QPluginLoader` мълчаливо отказваше да го зареди → **всички core нодове
изчезваха** от GUI палитрата И от headless регистрацията. Причина: 12 widget
класа и 2 connector TU-та не бяха в build-а.

**Тест А — брой недефинирани символи:**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster
for b in build_qt5 build_qt6; do
  echo -n "$b: "
  ldd -r $b/bin/libDemoNodeEditorNodesCorePlugin.so 2>&1 | grep -c "undefined symbol"
done
```
**Очаквано: `0` и `0`**

**Тест Б — реален dlopen през Python:**
```bash
for b in build_qt5 build_qt6; do
  python3 -c "
import ctypes
try:
    ctypes.CDLL('$PWD/$b/bin/libDemoNodeEditorNodesCorePlugin.so')
    print('$b: dlopen OK')
except OSError as e:
    print('$b: dlopen FAILED:', e)
"
done
```
**Очаквано: `dlopen OK` и двата**

**Тест В — widget-ите наистина са в build-а:**
```bash
sed -n '/^set(EMBEDDED_WIDGETS/,/^)/p' src/plugins/demo_nodeditor_nodes_core/CMakeLists.txt | grep -c '\.cpp$'
```
**Очаквано: `12`** widget TU-та (12 заглавни файла + 1 `.ui`)

Покритите класове: `VideoGLBlitWidget`, `GamepadWidget`, `AudioSourceDataModelUI`,
`SystemMonitorWidget`, `GpuMonitorWidget`, `PlutoSdrWidget`, `JackDetectWidget`,
`PcapWidget`, `NetworkSourceWidget`, `NetworkSinkWidget`, `FilePlaybackWidget`,
`FileRecordWidget`.

**Как разбираш, че е изпълнено:** 0 undefined symbols И успешен dlopen. Ако
тест А е чист, но в GUI палитрата няма core нодове — проблемът е другаде.

---

# Фичър 7 — `INodeProvider` се открива

**Какво е (бъг).** Търсенето беше с `qobject_cast<INodeProvider*>` + IID, но
`INodeProvider` **не е QObject** и не е обявен с `Q_INTERFACES`, така че
`qt_metacast` никога не можеше да го намери. Lookup-ът винаги връщаше празно →
headlessreportваше **всеки** тип нод като нерегистриран.

**Тест А — няма признаци за проблема:**
```bash
cd build_qt5/bin && unset QT_QPA_PLATFORM
./DaqsterHeadless --run ../../tests/data/video_effect_chain.flow --log-console-enabled 1 --log-level Info 2>&1 \
  | grep -iE "skipping unregistered|unregistered"
```
**Очаквато: празно (0 реда)**

**Тест Б — положителен:**
```bash
./DaqsterHeadless --run ../../tests/data/video_effect_chain.flow --log-console-enabled 1 --log-level Info 2>&1 \
  | grep "Discovered INodeProvider"
```
**Очаквано: 1 ред**

**Тест В — интерфейсът НЕ е QObject (важно!):**
```bash
grep -n "class INodeProvider" src/plugins/common/capabilities/INodeProvider.h
```
**Очаквано: без `: public QObject`**

> Ако го направиш QObject, билдът пада с
> *„'QObject' is an ambiguous base of 'Daqster::DemoNodeEditorNodesCoreObject'"*.
> Проверено — не опитвай.

**Тест Г — 3-те call-site-а ползват dynamic_cast:**
```bash
grep -ln "nodeProviders()" \
  src/frame_work/base/src/engine/HeadlessEngine.cpp \
  src/plugins/node_editor_ide/NodeEditorIdeObject.cpp \
  src/plugins/node_editor_ide/RuntimeShell.cpp
```
**Очаквано: и трите файла**

**Как разбираш, че е изпълнено:** тест А е празен, тест Б дава 1 ред, тест В
потвърждава че не е QObject.

---

# Фичър 8 — Headless върви на QApplication/offscreen

**Какво е.** `registerModel()` **инстанцира всеки модел само за да прочете
`name()`** (registry key идва от `static Name()` или `creator()->name()`).
Моделите правят widget-и в конструктора си → на `QCoreApplication` първият
widget счупва процеса с
*„QWidget: Cannot create a QWidget without QApplication"*.

Решението (вариант A): headless върви на истински `QApplication`, но с
`offscreen` platform plugin. Widget-ите са реални, нито един прозорец не се
показва, display server не е нужен.

**Тест А — вход 1 (отдерен binary):**
```bash
cd build_qt5/bin && unset QT_QPA_PLATFORM
./DaqsterHeadless --run ../../tests/data/number_graph.flow --log-console-enabled 1 --log-level Info 2>&1 | tail -7
```
**Очаквано:**
```
Headless mode: loading flow "<path>"
HeadlessEngine: loading flow: "<path>"
HeadlessEngine: Discovered INodeProvider plugin
HeadlessEngine: loaded nodes= 2 connections= 1
HeadlessEngine: flow loaded successfully
Flow loaded successfully, entering event loop
```

**Тест Б — вход 2 (Daqster с флаг):** → виж Фичър 9

**Тест В — video flow:**
```bash
./DaqsterHeadless --run ../../tests/data/video_effect_chain.flow --log-console-enabled 1 --log-level Info 2>&1 | grep "loaded nodes"
```
**Очаквано: `loaded nodes= 4 connections= 3`**

**Тест Г — и на Qt6:**
```bash
cd ../../build_qt6/bin && unset QT_QPA_PLATFORM
./DaqsterHeadless --run ../../tests/data/number_graph.flow --log-console-enabled 1 --log-level Info 2>&1 | grep "loaded nodes"
./DaqsterHeadless --run ../../tests/data/video_effect_chain.flow --log-console-enabled 1 --log-level Info 2>&1 | grep "loaded nodes"
```
**Очаквано: `2/1` и `4/3`**

**Тест Д — коректно затваряне (след ~20s):**
```bash
timeout 20 ./DaqsterHeadless --run ../../tests/data/video_effect_chain.flow --log-console-enabled 1 --log-level Info 2>&1 | tail -5
```
**Очаквано:**
```
HeadlessEngine: stopping all nodes
HeadlessEngine: stopping node: "Video File Source" (id= 0 )
HeadlessEngine: stopping node: ...
```
и exit code 0. **Всеки** нод трябва да бъде спрян.

**Тест Е — уважава изричен platform (пълна свобода на потребителя):**
```bash
cd ../build_qt5/bin && unset QT_QPA_PLATFORM
QT_QPA_PLATFORM=minimal ./DaqsterHeadless --run ../../tests/data/number_graph.flow --log-console-enabled 1 --log-level Info 2>&1 | head -3
```
**Оказва дали е използван зададеният `minimal`, а не `offscreen`.**

**Какво да гледаш в кода:**
- `src/frame_work/base/src/engine/HeadlessApp.h` — функцията `configureHeadlessPlatform()`
- `src/apps/DaqsterHeadless/main.cpp` — вика я **преди** `QApplication`
- `src/apps/Daqster/main.cpp` — същото

**Как разбираш, че е изпълнено:** всички тестове минават и рожбата е на
`HeadlessEngine`, не на RuntimeShell.

---

# Фичър 9 — `Daqster --headless --run` реално стига до своя клон

**Какво е (бъг, открит при изготвянето на чеклиста).** Ранният парсър течеше
преди да съществува `QCoreApplication` инстанция, а
`QCoreApplication::arguments()` връща **празен списък** в това състояние.
Парсерът виждаше нула аргументи → условието винаги лъжа → целият
`if (headlessMode && runMode)` клон беше **мъртъв код**. `Daqster --headless
--run` мълчаливо падаше в GUI режим и зареждаше flow-а през RuntimeShell.

**Тест (това е и регресионният тест):**
```bash
cd build_qt5/bin && unset QT_QPA_PLATFORM
./Daqster --headless --run ../../tests/data/number_graph.flow --log-console-enabled 1 --log-level Info 2>&1 | head -3
```
**Очаквано:**
```
Headless mode: loading flow "../../tests/data/number_graph.flow"
HeadlessEngine: loading flow: "../../tests/data/number_graph.flow"
```

**Ако видиш това — регресия:**
```
loadFlow: loaded "../../tests/data/number_graph.flow" nodes= 2 connections= 1
```

**Тест Б — и на Qt6:**
```bash
cd ../../build_qt6/bin && unset QT_QPA_PLATFORM
./Daqster --headless --run ../../tests/data/number_graph.flow --log-console-enabled 1 --log-level Info 2>&1 | grep "HeadlessEngine: loaded"
```
**Очаквано: `HeadlessEngine: loaded nodes= 2 connections= 1`**

**Проверка на поправката в кода:** `src/apps/Daqster/main.cpp`, ред ~79. Трябва
да има ръчно сглобен `QStringList` от `argc`/`argv`, **не**
`QCoreApplication::arguments()`.

**Как разбираш, че е изпълнено:** изходът започва с `Headless mode:` и
`HeadlessEngine:`, не с `loadFlow:`.

---

# Фичър 10 — OpenGL работи в headless

**Какво е.** `VideoGLContextManager` (`src/plugins/common/GL/`) използва
`QOpenGLContext` + `QOffscreenSurface` — **без прозорец**. Проверено емпирично,
че `offscreen` дава пълен hardware GL и в Qt 5.15, и в Qt 6.9.

**Тест А — генерирай тестово видео:**
```bash
ffmpeg -y -f lavfi -i "testsrc=size=320x240:rate=30:duration=3" -c:v mpeg4 -q:v 5 /tmp/testsrc.mp4
```

**Тест Б — flow с реален файл и `autoStart`:**
Копирай `tests/data/video_effect_chain.flow` и:
- сложи пътя в `"filePath": ""` → `"/tmp/testsrc.mp4"`
- добави `ui` секция с `"autoStart": true` за нодовете

**Тест В — рън и търсене на renderer:**
```bash
cd build_qt5/bin && unset QT_QPA_PLATFORM
timeout 20 ./DaqsterHeadless --run /tmp/gl_flow.flow --log-console-enabled 1 --log-level Info 2>&1 \
  | grep -iE "renderer|hardwareGL"
```
**Очаквано:**
```
VideoGLContextManager | renderer=Mesa Intel(R) Iris(R) Xe Graphics (TGL GT2) hardwareGL=yes
```

**Какво доказва този лог:** логът идва изключително от `setInData` (проверено и
в трите файла, които го викат), не от конструктор. Тоест **кадрите наистина
пристигат** и GPU пътят се взема.

**Какво НЕ е доказано:** че видео нодовете изпълняват дълга серия кадри без
грешки. Фикстурите в `tests/data/` нямат медиа и нямат `autoStart`, така че
стандартните тестове не стигат до GL обработка.

**Как разбираш, че е изпълнено:** логът излиза. Ако излезе
`hardwareGL=no`, машината ползва софтуерен renderer — тогава ефектите падат
на CPU по дизайн (`useGpu = GpuOrCpu && hasHardwareGL()`), не е бъг.

---

# Фичър 11 — GUI не е регрегирало

**Какво е.** Всичко в бранча пипа пътите, които минават и през GUI-то. Това е
задължителна проверка, не фичър по себе си.

**Тест:**
```bash
cd build_qt5/bin && unset QT_QPA_PLATFORM
timeout 15 ./Daqster --log-console-enabled 1 --log-level Debug > /tmp/gui.txt 2>&1
echo "exit=$?"
grep -c "Show window" /tmp/gui.txt
grep -c "PLUGIN METADATA" /tmp/gui.txt
sed -n '/Begin registered hashes/,/End registered hashes/p' /tmp/gui.txt | grep -c '"'
```
**Очаквано:**
```
exit=124
1
5
5
```

**Обяснение за `exit=124`:** това е **правилното** поведение. `timeout` убива
процеса, защото GUI-то чака вход. Ако получиш `exit=0` или друг код —
приложението е паднало.

Повтори за `build_qt6/bin`.

**Как разбираш, че е изпълнено:** прозорецът се отваря, 5 plugin-а се намират,
5 хеша се регистрират.

---

# Фичър 12 — Unit тестове

**Тест:**
```bash
cd build_qt5 && QT_QPA_PLATFORM=offscreen ctest
cd ../build_qt6 && QT_QPA_PLATFORM=offscreen ctest
```
**Очаквано и на двете:**
```
100% tests passed, 0 tests failed out of 11
```

**Как разбираш, че е изпълнено:** 11/11 на двете Qt версии. Ако падне —
спри, всичко друго е безсмислено.

> Unit тестовете за **самия REQ** не са правени — AC7 е отложен по
> инструкцията „НОВИ ТЕСТОВЕ СТОП".

---

# Карта: фичър → commit

| Фичър | Commit |
|---|---|
| 1. Миграция `src/core` + `src/gui` | `922a7aa` |
| 2. Билд Qt5/Qt6 | `c527eba` |
| 3. PUBLIC линковане | `4ae1963` |
| 4. Вградени node модели | `9aa85cc` |
| 5. Discovery vs `.ini` | `628a8ee` |
| 6. `dlopen` на core plugin | `1e528c9` |
| 7. `INodeProvider` discovery | `9288b84` |
| 8. Headless на QApplication | `592c3ad` |
| 9. `--headless` реален | `dd3f9e8` |
| 10. OpenGL в headless | няма commit — измерено, не е променяно |

---

# Обобщение: статус на 7-те AC

| AC | Текст | Статус | Кой фичър го покрива |
|---|---|---|---|
| 1 | Архитектурен шаблон core/gui, документиран | **частично** | Фичър 1, 4. Остават 9 мъртви include-а (вж. Фичър 4) и 10 модела с widget-и в core |
| 2 | Headless без Qt Widgets/OpenGL | **изпълнено (функционално)** | Фичър 8, 10. Формулировката е твоя от 2026-09-07; GL работи |
| 3 | Factory методите връщат nullptr в headless | **изпълнено** | `embeddedWidget()` се вика само от GUI — 7 места в `NodeEditorIdeObject`, **0** в `src/frame_work/` и `src/apps/` |
| 4 | Същият flow с и без GUI | **изпълнено** | Фичър 8. `number_graph` → `2/1` и през `HeadlessEngine`, и през `RuntimeShell` |
| 5 | (Опционално) REST API | **не е правено** | извън обхвата |
| 6 | Документация на шаблона | **частично** | `core-gui-split.md` описва целевия шаблон, но не и реалното състояние |
| 7 | Тестове | **отложено** | по твоята инструкция |

---

# Какво НЕ е в бранча (да не го чакаш)

1. **`NodeWidgetFactory` е мъртъв код.** `registerDefaultWidgetCreators()` е
   дефиниран и никога не извикан; `createWidget()` има **0** call sites;
   `getWidgetFactory()` не съществува. Реалният seam е вграденият в QtNodes
   `embeddedWidget()`.

2. **10 от 22 core модела правят widget в конструктора си.** При 4 от тях
   настройките **живеят в widget-а** — напр. `CustomShaderNode::save()` чете
   `m_glslEditor->toPlainText()`, `StreamSourceNode::start()` чете URL от
   `m_urlEdit->text()`. Махането им изисква програмен модел на състоянието за
   всеки — това е **редизайн**, не пренаписване.

3. **Това е твое изрично решение**, записано в `HeadlessApp.h:12-17`:
   *„headless = no canvas, NOT no widgets"* — core plugin-ят **умишлено** пази
   widget класовете си.

4. **Предупреждение в лога при видео:**
   ```
   [WRN] Setting a new default format with a different version or profile
         after the global share context is created may cause issues with
         context sharing.
   ```
   `VideoGLBlitWidget.cpp:102` вика `QSurfaceFormat::setDefaultFormat()` в
   конструктора. В headless widget-ът се конструира, но не се показва, а
   промяната идва след общия share context. Безобидно за offscreen пътя.

5. **Нищо не е merge-вато в `develop_pre`, нищо не е push-вато.**
   `develop_pre` си остава локален (11 локални commit-а), затова и бранчът не
   е push-нат — иначе би изтеглил и тях.

6. **REQ файлът е непроменен** — още пише `Коммити: (pending commit)` и няма
   чекнати AC-та. Чака теб.
