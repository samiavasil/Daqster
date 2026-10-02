# REQ-SW-PL-051 — Чеклист за проверка на бранча

> ## ⚠️ ИСТОРИЧЕСКИ СНИМЪК — 2026-09-26
>
> Чеклист от по-ранна точка на бранча, запазен за проследимост. **Не е текущо
> pending work.** Ключовите му изводи са впоследствие изместени:
>
> | Тук | Реално днес |
> |---|---|
> | „12 от 22 модела връщат `nullptr`; 10 още правят widget в конструктора" | **28 от 30** разделени; неразделени са **2** — `VideoOutput` (отложен за REQ-SW-PL-053) и `AudioDisplayObsolete` |
> | AC 1 „**частично**" — `core-gui-split.md` „описва factory, който никога не е вързан" | factory-ят е вързан през `IWidgetProvider`; документът е пренаписан спрямо реалното дърво |
> | AC 2 „**изпълнено**" | **БЛОКИРАН** — headless binary без QtWidgets не е постижим (`QApplication` в `NodeRunner`; 7 QtWidgets TU в `_core` заради `AudioDisplayObsolete`) |
> | AC 3 „`embeddedWidget()` се вика само от `NodeEditorIdeObject` (7 места)" | вече има кеширан accessor; `embeddedWidget()` е само fallback-ът в `ChatGraphModel::nodeWidget()` |
> | `Daqster --run` / `Daqster --headless` | `NodeRunner --run` / `NodeRunner --headless --run` (един dual-mode бинарник) |
>
> Актуално: `docs/Architecture/core-gui-split.md` и REQ файлът на
> `REQ-SW-PL-051-core-gui-separation-headless.md`.

- **Бранч:** `feat/REQ-SW-PL-051-core-gui-separation`
- **Base:** `develop_pre` @ `9dea41f`
- **Състояние:** working tree чист, нищо не е push-вано, нищо не е merge-вато
- **Дата на чеклиста:** 2026-09-26
- **Командите са изпълнени и потвърдени.** Всички „очаквани“ резултати са реално измерени.

---

## 0. Какво е бранчът supposed to прави — накратко

REQ-SW-PL-051 иска **headless изпълнение на flow** — същият `.flow` файл да тръгва
и без GUI. Реализацията избра **вариант A**: headless върви на `QApplication` с
`offscreen` platform (истински widget-и, но без прозорец и без display server).

По пътя се оказаха **5 реални бъга**, които спираха изобщо да работи — всички са
поправени в бранча. Плюс 1 нов бъг, открит при изготвянето на този чеклист.

**Важно:** бранчът съдържа и голяма начална миграция (`c527eba`, `922a7aa`),
която е била в бранча преди тези 5 фикса.

---

## 1. Списък на commit-ите (9 бр., най-стар → най-нов)

| # | hash | Какво прави |
|---|---|---|
| 1 | `c527eba` | CMake include paths, липсващи заглавни файлове, namespace поправки — билдът минава на Qt5/Qt6 |
| 2 | `922a7aa` | Изтриване на старите `src/core/` и `src/gui/`; всичко под `src/frame_work/` |
| 3 | `9aa85cc` | Нови widget-модели за headless: NumberSource, NumberDisplay, Modulo, ArithmeticLogic |
| 4 | `4ae1963` | `PluginDependencyManager`: Qt и вътрешни target-и се линкват PUBLIC |
| 5 | `628a8ee` | **Бъг:** discovery филтрираше по `.ini` файл, не по заредени plugin-и |
| 6 | `1e528c9` | **Бъг:** core plugin имаше 31 недефинирани символа → `dlopen` падаше |
| 7 | `9288b84` | **Бъг:** `INodeProvider` не можеше да се намери → всички нодове „нерегистрирани" |
| 8 | `592c3ad` | Headless на `QApplication`/offscreen вместо `QCoreApplication` |
| 9 | `dd3f9e8` | **Бъг:** `Daqster --headless --run` не стигаше до своя клон (точното условие е тук) |

Плюс 1 непоправен дефект, документиран в т. 6.

---

## 2. Подготовка — как да build-ваш

```bash
cd /mnt/Builder/Projects/samiavasil/daqster

# Qt5
cmake -S . -B build_qt5 -DCMAKE_PREFIX_PATH=/mnt/Builder/bin/Linux/Qt/5.15.2/gcc_64 -DDAQSTER_BUILD_TESTS=ON
cmake --build build_qt5 -j8

# Qt6
cmake -S . -B build_qt6 -DCMAKE_PREFIX_PATH=/mnt/Builder/bin/Linux/Qt/6.9.2/gcc_64 -DDAQSTER_BUILD_TESTS=ON
cmake --build build_qt6 -j8
```

> Не използвай `scripts/build.sh` за итерация — той винаги минава с `--clean`.

**Проверка:** 0 errors, 0 warnings на двете Qt версии. Ако има warning-и — не
продължавай, ще ги разгледаме.

---

## 3. Тестове на Unit ниво

```bash
cd build_qt5 && QT_QPA_PLATFORM=offscreen ctest
cd ../build_qt6 && QT_QPA_PLATFORM=offscreen ctest
```

**Очаквано (потвърдено и на двете):**
```
100% tests passed, 0 tests failed out of 11
```

Ако тук падне нещо — всичко останало е безсмислено, връщай се на т. 2.

---

## 4. Т.5 — Bug: plugin discovery след първия рън

**Какво беше:** `SearchForPlugins()` подаваше на `discoverPlugins()` списък с
вече-описани plugin-и, който идва от `.ini` файл, зареден при стартиране. Този
файл преживява рестартите. Така че **от втория рън нататък всеки plugin на
диска съвпадаше със записан hash → „вече известен" → пропускан**. Нищо не се
зареждаше, без нито един ред в лога (нямаше какво да се докладва).

**Тест — стъпка 1: изтрий persistence-а**
```bash
rm -f ~/.config/DaqsterHeadless/daqster_qt5.ini ~/.config/Daqster/daqster_qt5.ini
cd build_qt5/bin && unset QT_QPA_PLATFORM
./DaqsterHeadless --run ../../tests/data/number_graph.flow --log-console-enabled 1 --log-level Info
```
**Очаквано:** `HeadlessEngine: loaded nodes= 2 connections= 1`

**Тест — стъпка 2: пусни отново, без да триеш нищо**
```bash
./DaqsterHeadless --run ../../tests/data/number_graph.flow --log-console-enabled 1 --log-level Info
```
**Очаквано: абсолютно същото** — `loaded nodes= 2 connections= 1`

> Ако стъпка 2 даде празно или „skipping", фиксът е извън. **Това е точно
> регресion тестът за този commit** — не го пропускай.

**Тест — същото за GUI-то**
```bash
./Daqster --log-console-enabled 1 --log-level Debug 2>&1 | grep -c "PLUGIN METADATA"
```
**Очаквано: `5`** (QtCoinTrader, NodeEditorIde, NodesGui, NodesCore, RequirementsManager)

> Логът на GUI излиза **само на `--log-level Debug`**. На `Info` няма изход —
> това не е бъг.

---

## 5. Т.6 — Bug: core plugin-ът не можеше да бъде зареден

**Какво беше:** plugin-ът излизаше с 31 недефинирани символа. `QPluginLoader`
мълчаливо отказваше да го зареди → **всички core нодове изчезваха** и от GUI
палитрата, и от headless регистрацията. Причина: 12 widget класа и 2 connector
TU-та не бяха в build-а.

**Тест А — брой недефинирани символи**
```bash
cd /mnt/Builder/Projects/samiavasil/daqster
for b in build_qt5 build_qt6; do
  echo -n "$b: "
  ldd -r $b/bin/libDemoNodeEditorNodesCorePlugin.so 2>&1 | grep -c "undefined symbol"
done
```
**Очаквано: `0` и `0`**

**Тест Б — реален dlopen през Python**
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

> Ако тук е OK, но в GUI палитрата няма core нодове — проблемът е другаде.

---

## 6. Т.7 — Bug: `INodeProvider` не можеше да се открие

**Какво беше:** търсенето беше с `qobject_cast<INodeProvider*>` + IID, но
`INodeProvider` **не е QObject** и не е обявен с `Q_INTERFACES`, така че
`qt_metacast` никога не можеше да го намери. Lookup-ът винаги връщаше празно →
headlessreportваше **всеки** тип нод като нерегистриран.

**Тест — търсене в лога за признаци на проблема**
```bash
cd build_qt5/bin && unset QT_QPA_PLATFORM
./DaqsterHeadless --run ../../tests/data/video_effect_chain.flow --log-console-enabled 1 --log-level Info 2>&1 \
  | grep -iE "skipping unregistered|unregistered"
```
**Очаквано: празно** (нула реда)

**Тест — положителен**
```bash
./DaqsterHeadless --run ../../tests/data/video_effect_chain.flow --log-console-enabled 1 --log-level Info 2>&1 \
  | grep "Discovered INodeProvider"
```
**Очаквано: 1 ред** — `HeadlessEngine: Discovered INodeProvider plugin`

**Какво да гледаш в кода:**
- `src/plugins/common/capabilities/INodeProvider.h` — **трябва да НЕ е QObject**
- `PluginRegistry::nodeProviders()` и `QPluginManager::nodeProviders()` —
  използват `dynamic_cast`, не `qobject_cast`
- 3-те call-site-а: `HeadlessEngine.cpp`, `NodeEditorIdeObject.cpp`, `RuntimeShell.cpp`

> Ако направиш `INodeProvider` наследник на `QObject`, билдът пада с
> *„'QObject' is an ambiguous base of 'Daqster::DemoNodeEditorNodesCoreObject'"*.
> Това е проверено — не опитвай.

---

## 7. Т.8 — Headless върви на QApplication/offscreen

**Какво беше:** `NodeDelegateModelRegistry::registerModel()` **инстанцира всеки
модел само за да прочете `name()`**. Моделите правят widget-и в конструктора
си → на `QCoreApplication` първият widget счупва процеса с
*„QWidget: Cannot create a QWidget without QApplication"*.

**Тест А — двата headless входа**
```bash
cd build_qt5/bin && unset QT_QPA_PLATFORM

# вход 1
./DaqsterHeadless --run ../../tests/data/number_graph.flow --log-console-enabled 1 --log-level Info
# вход 2
./Daqster --headless --run ../../tests/data/number_graph.flow --log-console-enabled 1 --log-level Info
```
**Очаквано и за двата (редът на редовете е важен!):**
```
Headless mode: loading flow "<path>"
HeadlessEngine: loading flow: "<path>"
HeadlessEngine: Discovered INodeProvider plugin
HeadlessEngine: loaded nodes= 2 connections= 1
HeadlessEngine: flow loaded successfully
Flow loaded successfully, entering event loop
```

> **Критично за разликаване на двата пътя:**
> - `HeadlessEngine: loaded nodes=` → правилният headless път
> - `loadFlow: loaded ... nodes=` (без „Headless") → RuntimeShell, т.е. **грешният**
>   път; `--headless` не е сработил и програмата е паднала в GUI режим

**Тест Б — video flow**
```bash
./DaqsterHeadless --run ../../tests/data/video_effect_chain.flow --log-console-enabled 1 --log-level Info
```
**Очаквано:** `loaded nodes= 4 connections= 3`

**Тест В — и на Qt6**
```bash
cd ../../build_qt6/bin && unset QT_QPA_PLATFORM
./DaqsterHeadless --run ../../tests/data/number_graph.flow --log-console-enabled 1 --log-level Info 2>&1 | grep "loaded nodes"
./DaqsterHeadless --run ../../tests/data/video_effect_chain.flow --log-console-enabled 1 --log-level Info 2>&1 | grep "loaded nodes"
./Daqster --headless --run ../../tests/data/number_graph.flow --log-console-enabled 1 --log-level Info 2>&1 | grep "HeadlessEngine: loaded"
```
**Очаквано:** `2/1`, `4/3`, `2/1`

**Тест Г — коректно затваряне**
```bash
# след ~20s timeout трябва да се види:
HeadlessEngine: stopping all nodes
HeadlessEngine: stopping node: "Video File Source" (id= 0 )
...
```
**Очаквано:** всички нодове се спират, exit code 0

**Какво да гледаш в кода:**
- `HeadlessApp.h` / `.cpp` — функцията `configureHeadlessPlatform()`
- `DaqsterHeadless/main.cpp` и `Daqster/main.cpp` — извикват я **преди**
  създаването на `QApplication`
- Функцията **запазва** вече зададен `QT_QPA_PLATFORM` — проверка:
  `QT_QPA_PLATFORM=minimal ./DaqsterHeadless --run ...` трябва да ползва `minimal`

---

## 8. Т.9 — Bug: `Daqster --headless --run` не стигаше до своя клон

**Това е бъг, открит при изготвянето на този чеклист, и е поправен в `dd3f9e8`.**

**Какво беше:** ранният парсър вървеше преди да съществува `QCoreApplication`
инстанция. А `QCoreApplication::arguments()` връща **празен списък**, когато
няма инстанция. Парсерът виждаше нула аргументи → `headlessMode` и `runMode`
винаги `false` → целият `if (headlessMode && runMode)` клон беше мъртъв код.
`Daqster --headless --run` мълчаливо падаше в GUI режим.

**Тест (това е и регресионният тест за `dd3f9e8`)**
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

**Проверка на поправката в кода:** `src/apps/Daqster/main.cpp`, ред ~79.
Трябва да има ръчно сглобен `QStringList` от `argc`/`argv`, а не
`QCoreApplication::arguments()`.

---

## 9. Регресия: GUI-то трябва да работи както преди

```bash
cd build_qt5/bin && unset QT_QPA_PLATFORM
timeout 15 ./Daqster --log-console-enabled 1 --log-level Debug > /tmp/gui.txt 2>&1
```
**Очаквано:**
- exit code **124** (timeout го е убил = програмата стои и чака → прозорецът е отворен)
- `Show window` — 1 път
- `PLUGIN METADATA` — **5** пъти
- между `Begin registered hashes` и `End registered hashes` — **5** хеша

Повтори и за Qt6 (`build_qt6/bin`).

> Ако GUI-то не тръгва — това е по-сериозно от всичко в headless.

---

## 10. OpenGL в headless — какво е доказано и какво не

**Доказано емпирично** (probe + реален рън с видео кадри):
`offscreen` в Qt 5.15 **и** Qt 6.9 дава пълен hardware OpenGL.

```bash
# 1) генерирай тестово видео (ако още нямаш)
ffmpeg -y -f lavfi -i "testsrc=size=320x240:rate=30:duration=3" -c:v mpeg4 -q:v 5 /tmp/testsrc.mp4
```

Създай flow с реален файл и `autoStart` (копирай `tests/data/video_effect_chain.flow`
и смени `"filePath": ""` с пътя до файла; добави `ui` секция с
`"autoStart": true` за нодовете), после:
```bash
cd build_qt5/bin && unset QT_QPA_PLATFORM
timeout 20 ./DaqsterHeadless --run /tmp/gl_flow.flow --log-console-enabled 1 --log-level Info 2>&1 \
  | grep -iE "renderer|hardwareGL"
```
**Очаквано:**
```
VideoGLContextManager | renderer=Mesa Intel(R) Iris(R) Xe Graphics (TGL GT2) hardwareGL=yes
```

**Какво значи това:** кадрите наистина пристигат и GPU пътят се взема. Логът
идва изключително от `setInData` (проверено и в 3-те файла), не от конструктор.

**Какво НЕ е доказано:** че видео нодовете изпълняват дълга серия кадри
без грешки. Фикстурите в `tests/data/` нямат медиа и нямат `autoStart`, така
че рамките на стандартните тестове не стигат до GL обработка.

---

## 11. Статус на 7-те AC от REQ файла

| AC | Текст | Статус | Обосновка |
|---|---|---|---|
| **1** | Архитектурен шаблон core/gui, документиран в `docs/Architecture/` | **частично** | `core-gui-split.md` съществува, обаче описва factory, който никога не е вързан. 12 от 22 модела вече връщат `nullptr`; 10 все още правят widget в конструктора (по изрично решение, т.12) |
| **2** | Headless стартиране на flow без Qt Widgets/OpenGL | **изпълнено — функционално** | Проверено с реален headless рън. Формулировката „без OpenGL" е твоя от 2026-09-07; GL всъщност работи (т.10) |
| **3** | GUI factory методите връщат nullptr в headless | **изпълнено** | `embeddedWidget()` се вика **само** от `NodeEditorIdeObject` (7 места). В `src/frame_work/` и `src/apps/` — **0** извиквания. Headless не пита |
| **4** | Същият flow работи с и без GUI | **изпълнено** | `number_graph` → `2/1` и през `HeadlessEngine`, и през `RuntimeShell` (`Daqster --run` без `--headless`) |
| **5** | (Опционално) REST API | **не е правено** | Извън обхвата, опционално |
| **6** | Документация на шаблона | **частично** | `core-gui-split.md` описва целевия шаблон, но не и реалното състояние (че factory-то е мъртво) |
| **7** | Тестове | **отложено** | По твоята инструкция „НОВИ ТЕСТОВЕ СТОП" |

---

## 12. Известни ограничения — прочети преди да решиш

**А) `NodeWidgetFactory` е мъртъв код.**
Файлът `src/plugins/demo_nodeditor_nodes_gui/NodeWidgetFactory.h` съществува,
но:
- `registerDefaultWidgetCreators()` е дефиниран и **никога не извикан**
- `createWidget()` има **0** call sites
- `getWidgetFactory()` от документацията **не съществува** в кода

Реалният начин за създаване на widget е вграденият в QtNodes `embeddedWidget()`.
Ако това е проблем за теб — кажи, че е отделен REQ.

**Б) 10 от 22 core модела правят widget в конструктора си.**
`CameraSourceNode`, `CustomShaderNode`, `FrameSamplerNode`, `StreamSourceNode`,
`VideoEffectNode`, `VideoFileSourceNode`, `VideoOutputNode`, `ConsoleDataModel`,
`LLamaModelDataModel`, `NumberSourceDataModel`.

При 4 от тях настройките **живеят в widget-а**, напр.:
- `CustomShaderNode::save()` чете `m_glslEditor->toPlainText()` и `m_sliders[i]->value()`
- `StreamSourceNode::start()` чете URL от `m_urlEdit->text()`
- `CameraSourceNode::save()` чете `m_deviceCombo->currentData()`
- `NumberSourceDataModel::save()` чете 2 widget члена

Пълното махане изисква програмен модел на състоянието за всеки от тях — това е
редизайн, не пренаписване.

**В) Това е изрично твое решение**, записано в `HeadlessApp.h:12-17`:
*„headless = no canvas, NOT no widgets"* — core plugin-ят **умишлено** пази
widget класовете си.

**Г) `QSurfaceFormat` предупреждение в headless**
```
[WRN] Setting a new default format with a different version or profile after
      the global share context is created may cause issues with context sharing.
```
`VideoGLBlitWidget.cpp:102` вика `QSurfaceFormat::setDefaultFormat()` в
конструктора. В headless widget-ът се конструира, но не се показва, а промяната
идва след създаването на общия share context. Безобидно за offscreen пътя.

**Д) Същото и за Qt 5.15** — предупреждението от Г се появява и на двете версии.

---

## 13. Бързо ръководство: „поправи ли се?"

Ето редът, по който да вървиш, ако нещо се счупи:

1. `ctest` и на двете Qt версии → ако пада, проблемът е в кода, не в конфигурацията
2. `ldd -r` на core plugin-а → ако има undefined symbols, виж т.5
3. `DaqsterHeadless --run number_graph` → ако празно, виж т.4 (discovery)
4. Същото → ако „unregistered", виж т.6 (INodeProvider)
5. Същото → ако пада с QWidget грешка, виж т.7
6. `Daqster --headless --run` → ако дава `loadFlow:` вместо `HeadlessEngine:`, виж т.8
7. GUI → ако не тръгва, виж т.9

---

## 14. Какво остава нерешено

- `develop_pre` е локален и **не е push-ват** — така че и този бранч не е
  push-нат (иначе би изтеглил 11-те commit-а на `develop_pre`)
- Нищо не е merge-вато в `develop_pre`
- REQ файлът още пише `Коммити: (pending commit)` и няма чекнати AC-та
- AC3 не е пипнат в кода (виж т.12 А) — по твоя избор чака решение
