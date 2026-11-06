# Затваряне на сцена (scene close lifecycle)

Родител: [Node Editor](README.md) | [Архитектура](../README.md) |
[Индекс](../../index.md)
Виж още: [Capabilities](../capabilities.md) · [REST/remote control](../rest-api-remote-control.md) ·
[Flow формат](../formats/flow-format.md)

Отварянето на сцена имаше близнак само на хартия. `loadFlow()` съществуваше от
началото, а „затваряне" беше разпръснато по деструктори и по `stop()`-ове, които
не се викаха навсякъде. Резултатът беше реален дефект: при подмяна на сцена
(`File → Load Scene…`) `clearScene()` триеше моделите, без преди това да им е
викнат `stop()`, а плъгините не получаваха никакъв момент, в който да освободят
това, което държат извън нодовете (кеширани тегла, топли процеси). Всичко това
„работеше", докато процесът умре — тоест не работеше при hot-reload и при
повторно отваряне в един и същ процес.

`REQ-SW-PL-078` въвежда затварянето като **договор**, а не като един ред.
`REQ-SW-PL-082` прави същото за другата половина: отварянето пита плъгините
**преди** да спре и унищожи работещата сцена (виж „Отварянето е договор" по-долу).

## Договорът

```
затваряне на сцена
  1. IStoppable::stop()          всеки нод, източниците първи
  2. ISceneLifecycle::onSceneClosed()   всеки INodeProvider (optional hook)
  3. clearScene() / deleteNode()        чак сега моделите се унищожават
```

Редът е целият смисъл:

- **стъпка 1 преди стъпка 3** — модел, който се унищожи, докато работи, оставя
  нишка/процес/устройство срещу полу-мъртъв обект. `stop()` трябва да е
  идемпотентен, защото затварянето се вика и от „затвори сцената", и от
  „изход от приложението".
- **източниците първи** — `SceneClose::sceneStopOrder()` подрежда нодовете с
  топологично сортиране (Kahn) по връзките: нодове без входящ ръб (камери,
  файлове, микрофони, клавиатура) спират първи, консуматорите последни. Така
  веригата вижда „няма повече данни", преди да бъде спряна сама. Граф с цикъл не
  може да се подреди — остатъкът се спира в реда на id, защото затваряне не бива
  да остави работещ нод заради странен граф.
- **стъпка 2 между тях** — плъгинът вижда нодовете си **вече спрени** (нула живи
  контексти), но още живи като обекти. Това е единственият момент, в който може
  да реши съдбата на теглата.

## Отварянето е договор (REQ-SW-PL-082)

Затварянето получи договор, но отварянето остана „просто `load()`" — и в двата
non-IDE host-а редът беше **обратен**: `stopAllNodes()` + `notifySceneClosed()`
се викаха **преди** файлът да е прочетен, а `buildGraphModel()` унищожаваше
стария граф **преди** парсването. Тоест невалиден файл (сгрешен път, скъсан JSON,
сцена, която не може да работи на тази машина) убиваше работещата сцена, без да
зареди нищо. Сега редът е:

```
прочети → спри старото → зареди новото → питай плъгините → (при проблеми: не стартирай)
```

Присъдата **не е вето**: `REQ-SW-PL-082` смени семантиката от „отказвам да
заредя" на **„зарежда, маркира, не стартира"**. Сцена, за която плъгинът казва
„този модел липсва на тази машина", се **зарежда** — потребителят вижда графа,
който е поискал, и може да поправи пътя или да смени нода — но нодовете с
`autoStart` остават **спрени**, защото стартирането е това, което би изгърмяло.
Причината: отказът на зареждане беше по-скъп от проблема. Един липсващ файл
връщаше човека към празен екран, а графът, който вижда, е и единственото място,
на което може да поправи нещо.

Пита се optional capability-то `ISceneOpening` (виж
[capabilities.md](../capabilities.md) §ISceneOpening): `onSceneOpening(scene)`
връща **всички** причини, поради които сцената не може да се стартира, а празен
списък значи „няма проблеми". Причините са **структурирани** (`SceneOpenProblem`:
id на нода, име, съобщение и подсказка), не низове — така хостът може да маркира
визирания нод, а не само да покаже списък.

| Път | Къде се пита | Какво става при проблеми | Код |
|---|---|---|---|
| `NodeRunner --headless`, `loadFlow()` | последната врата, в `applySceneJson()` след филтрирането на непознатите типове | сцената се зарежда; `autoStartNodes()` се пропуска; `qCWarning` на проблем + `errorOccurred()` | `HeadlessEngine.cpp` `applySceneJson()` → `recordSceneOpening()`; `loadFlow()` стъпка 8 |
| `POST /api/flow` (headless и GUI) | същото, през `reloadFlow()` | 200 `{"applied": true, "started": false, "problems": […]}` | `ControlPlaneRouter.cpp` (`POST /api/flow`); `lastSceneProblems()` |
| `NodeRunner --run` (GUI), `loadFlow()` | `applySceneJson()` | сцената се зарежда; `RunRuntime()` не стартира `autoStart` нодовете | `RuntimeShell.cpp` `applySceneJson()` → `recordSceneOpening()`; `RunRuntime()` стъпка 6 |
| `POST /api/flow` (GUI runtime) | `reloadFlow()` → `applySceneJson()` | същото тяло като headless (един и същ router) | `RuntimeShell.cpp:reloadFlow()` |
| IDE `File → Load Scene…` | **след** `load()`, върху заредената сцена | сцената се зарежда и остава за редактиране; всеки проблем маркира нода си (`NodeValidationState::Error`), панелът под канваса изрежда всички, по един `qCWarning` на проблем; **без** модален диалог; нищо не се стартира | `NodeEditorIdeObject.cpp:loadSceneFromFile()` → `applySceneProblems()`; `SceneProblemsPanel` |

Защо питане на едно място, а не две: при ветото ранното питане имаше смисъл
(отказът не струваше нищо); при „зарежда, маркира, не стартира" всяко питане
преди `load()` пита за сцена, която после се филтрира (непознатите типове
отпадат), тоест отговорът може да е за нодове, които вече не съществуват.
Затова пита единственото място, което вижда **филтрираната** сцена —
`applySceneJson()` в двата host-а (и двете през `recordSceneOpening()`), а в IDE-то
— `loadSceneFromFile()` **след** `load()`.

### IDE-то: маркирай, не отказвай (`SceneProblemsPanel`)

IDE-то беше последният хост с вето: `File → Load Scene…` показваше
`QMessageBox::warning` и **не зареждаше** нищо, тоест човекът оставаше на празен
канвас — а графът е единственото място, на което липсващият път се поправя.
Сега `loadSceneFromFile()` пита **след** успешния `load()` (нодовете трябва да
съществуват, за да бъдат маркирани) и прави точно три неща:

1. **маркира** — за всеки проблем с непразен `nodeId` нодът получава
   `setValidationState({State::Error, message (+ " — " + hint)})`, което
   `NodeGraphicsObject` рисува върху тялото на нода; проблем без `nodeId` (за
   цялата сцена) само се логва;
2. **изрежда** — `applySceneProblems()` пълни `SceneProblemsPanel` под канваса
   (хедър `Проблеми в сцената (N)`, свит по подразбиране; двойно щракване или
   Enter върху ред излъчва `problemActivated(nodeId)` и хостът селектира нода и
   центрира изгледа върху него). Празен списък **скрива** панела — чистата сцена
   не губи височина;
3. **логва** — по един `qCWarning(lcNodeEditor)` на проблем, с id и текст.

Модален диалог няма: сцената остава отворена и редактируема, а нищо не се
стартира (`autoStart` е грижа на runtime host-овете, не на редактора). Затваряне
на сцената изчиства и панела, защото проблемите описват нодове, които вече не
съществуват.

Текстът на реда е същият като в логовете —
`SceneProblemsPanel::textForProblem()`: `<nodeName> (<nodeId>): <message> — <hint>`,
празните части се пропускат.

#### Живи валидационни грешки (не само при отваряне)

Панелът показва **две неща**, не едно:

1. проблемите от `ISceneOpening` при отваряне (присъдата, пазена в
   `m_sceneOpenProblems`);
2. **живите** валидационни грешки на нодовете — всичко с
   `NodeValidationState::State != Valid` в `NodeRole::ValidationState`.

Второто съществува, защото на канваса един червен нод **не казва нищо**:
съобщението е само tooltip, а иконката за проблема се рисува извън нода — това е
известен дефект на `nodeeditor` submodule-а и тук **не се поправя**. Докато той
съществува, панелът е единственото място, на което „червено" има прочетим текст.

`NodeEditorIdeObject::refreshNodeProblems()` обхожда `allNodeIds()`, чете
`NodeRole::Caption` и `NodeRole::ValidationState` и подава състоянията на
`NodeProblems::merge()` заедно с присъдата. Правилата (тестват се с чисти данни,
без граф — `NodeProblems.{h,cpp}`):

- ред се прави за всяко състояние, различно от `Valid` (`Warning` също), с
  текста на нода; празно съобщение получава резервен текст, защото „червено без
  текст" е точно дефектът, който се поправя;
- **дедупликация по (`nodeId`, `message`)**: `applySceneProblems()` маркира нода
  със същия текст, който после идва и като жива валидация — потребителят трябва
  да го види веднъж; при две копия оцелява това с `hint` (присъдата е по-богата);
- щом нодът стане `Valid`, редът му изчезва, а празен списък скрива панела.

Кога се вика: след `load()`, след `applySceneProblems()` и при сигналите на
графа (`nodeUpdated`, `nodeCreated`, `nodeDeleted`, `modelReset`).
`setValidationState()` излъчва `requestNodeUpdate()` → `nodeUpdated`, тоест
живата грешка стига до панела без polling. Изливите от `nodeUpdated` (нод, който
се обновява всеки кадър) се събират в един single-shot `QTimer` (0 ms) и
`NodeProblems::sameProblems()` пропуска презареждането, когато списъкът не се е
променил — панелът не се строи наново всеки кадър. Таймерът се спира при
затваряне на сцената; при затваряне `m_sceneOpenProblems` и панелът се изчистват.

Хостът пази присъдата в `m_lastSceneProblems` и я дава навън през
`RuntimeController::lastSceneProblems()` — така `POST /api/flow` казва и двете
неща в един отговор, вместо да избира между „успя" и „не успя":

```json
{"applied": true, "started": false,
 "problems": [{"nodeId": "3", "nodeName": "Chat",
               "message": "моделът липсва", "hint": "изтеглете го"}]}
```

`400 {"applied": false, "error": …}` остава само за документ, който хостът не
може да зареди изобщо (счупен JSON, невалидна структура). `started` е
`problems.isEmpty()`, а не „стартира ли се нещо" — сцена без `autoStart` нодове
също дава `started: true`.

Текстът се строи на едно място — `sceneOpeningMessage()`
(`src/frame_work/base/src/engine/SceneOpen.cpp`), по модела на
`unsupportedHeadlessMessage()` (`HeadlessEngine.cpp`) — и затова логът и
`errorOccurred()` казват едно и също: всички проблеми, един под друг, всеки с
нода, който визира (`<nodeName> (<nodeId>): <message> — <hint>`, празните части
се пропускат). Който иска данни, а не текст, ползва
`SceneOpenReport::problemsJson()` / `lastSceneProblems()`.

## Причината за затварянето (`SceneCloseReason`)

`onSceneClosed()` казва *че* сцената изчезва, но не и *защо* — а плъгин, който
пази тегла или топъл сървър, има нужда да различи подмяна от изрично затваряне.
Затова hook-ът получава `Daqster::SceneCloseReason`:

| Причина | Кой я подава | Какво значи |
|---|---|---|
| `Replace` | `loadFlow()`/`reloadFlow()` (и трите host-а), `POST /api/flow`, изход от приложението | идва нова сцена — същите модели ще потрябват отново |
| `ExplicitClose` | `File → Close Scene` (Ctrl+W), `RuntimeShell::closeFlow()`, `HeadlessEngine::closeFlow()` (`POST /api/flow/close`) | човекът поиска сцената да я няма |

**Причината е информация, не заповед.** Хостът казва *защо*, а какво остава в
паметта решава плъгинът — и неговата настройка (env, settings, UI) печели пред
причината. Нито един път не освобождава кеш автоматично: топъл кеш е `evictable`
и струва нищо, докато процесът живее, а връщането на паметта е изрично действие.
Какво причината **не** променя: всеки нод пак освобождава собствените си ресурси
(контексти, процеси, устройства) в `stop()` — това е договорът, не политика.

Съвместимост: `onSceneClosed(SceneCloseReason)` е **отделен** virtual слот след
стария `onSceneClosed()`, чиято имплементация по подразбиране просто препраща —
плъгин, компилиран без причина, продължава да получава hook-а си.

## Кой какво вика

| Път | Какво прави | Код |
|---|---|---|
| IDE `File → Close Scene` (Ctrl+W, бутон в toolbar-а) | пита при нестартирани промени → `stopSceneNodes()` → `notifySceneClosed()` → `clearScene()` | `NodeEditorIdeObject::closeCurrentScene()` |
| IDE `File → Load Scene…` | пита `ISceneOpening` (отказ = нищо не се пипа) → същото, без въпроса (сцената се подменя) | `NodeEditorIdeObject::loadSceneFromFile()` |
| `NodeRunner --run` (GUI), смяна на сцена | чете → `stopAllNodes()` → `notifySceneClosed()` → `clearScene()` → зарежда → пита `ISceneOpening` (проблеми = без `autoStart`) | `RuntimeShell::loadFlow/reloadFlow/closeFlow` |
| `NodeRunner --headless`, `POST /api/flow` | чете → `stopAllNodes()` → `notifySceneClosed()` → нов `DataFlowGraphModel` → зарежда → пита `ISceneOpening` (проблеми = без `autoStart`) | `HeadlessEngine::loadFlow/reloadFlow` |
| `POST /api/flow/close` | `closeSceneGraph()` = спиране + hook + изтриване на възлите | `HeadlessEngine::closeFlow`, `RuntimeShell::closeFlow` |
| Изход от приложението | `stopAllNodes()` при `aboutToQuit` → `ISceneLifecycle` → `DeInitialize()` | `AppBootstrap.cpp` |

Единствената реализация на стъпки 1–2 е `FrameworkCore`
(`src/frame_work/base/src/engine/SceneClose.h/.cpp`), за да не се разминават трите
host-а:

```cpp
Daqster::SceneCloseReport stopSceneNodes(QtNodes::DataFlowGraphModel& graph);
int  Daqster::notifySceneClosed();                     // от QPluginManager
Daqster::SceneCloseReport closeSceneGraph(QtNodes::DataFlowGraphModel& graph);
Daqster::SceneCloseReport stopAndNotifySceneClosed(QtNodes::DataFlowGraphModel* graph);
int  Daqster::clearGraphNodes(QtNodes::DataFlowGraphModel& graph);   // widget-free clearScene
```

`SceneCloseReport` носи `nodes/stopped/removed/notifiedProviders/stoppedIds` — за
един log ред, за REST отговора и за тестовете, вместо всеки host да брои сам.

## Два примитива за lifetime

**Правило: нито един нод не пипа `QThread`/`QProcess` наготово.** Животът на
нишка/процес минава през един от двата примитива в
`src/plugins/common/Threading/`, защото „спиране" на нишка и „спиране" на процес
са два различни договора:

| | `ThreadOwner<Worker>` | `ProcessOwner` |
|---|---|---|
| какво притежава | `QThread` + worker `QObject` (moveToThread) | един `QProcess` |
| как се спира | `quit()` → **join** с краен срок | `terminate()` → wait → `kill()` → wait |
| какво връща `stop()` | `NotRunning` / `Stopped` / `TimedOut` | `NotRunning` / `Stopped` / `Killed` / `TimedOut` |
| чужд/преизползван | — (винаги собствена нишка) | `setOwned(false)` / `adopt(p, false)` → **никога не се убива** |
| къде е | `Threading/ThreadOwner.h` (header-only шаблон) | `Threading/ProcessOwner.{h,cpp}` (в `DaqsterCommonLibrary`) |

Защо не един клас: нишката се **присъединява** (тя трябва да се върне от
`run()`; `terminate()` е опасен и се използва само като последна мярка), а
процесът се **моли да излезе** и има код на изход — `SIGTERM`/`WM_CLOSE` е
молба, която може да бъде игнорирана, и единственото, което не може, е
`SIGKILL`/`TerminateProcess`. Един общ клас би трябвало да има две
несъвместими семантики на „спиране" и флаг „може ли да се убие", тоест двете
разклонения пак щяха да са там — само че скрити.

### Правилата, които примитивите налагат (и защо)

- **`stop()` е идемпотентен.** Затварянето се вика и от „затвори сцената", и от
  изхода на приложението; второто извикване връща `NotRunning`, не втори join.
- **`ThreadOwner` вдига latch-а ПРЕДИ `quit()`.** `invoke()`/`alive()` минават
  през един предикат (latch → после pointer), така че задача, пристигнала по
  време на спирането, се **отказва**, вместо да се зашие в event loop-а на
  умираща нишка. След успешен `stop()` worker-ът е `nullptr`.
- **Join-ът има краен срок.** `quit()` → `wait(3000)`; ако не се прибере →
  `terminate()` → `wait(500)` → `TimedOut` + `qCWarning`. Нишка, която оцелее и
  след това, остава прикачена (разрушаването на работещ `QThread` вдига
  `abort()`), а `stop()` пак връща — никога безкрайно чакане.
- **Процесът се убива само ако е наш.** `setOwned(false)` (или `adopt(p,false)`
  за преизползван сървър) кара `stop()` да **забрави** процеса, без да го
  сигнализира — иначе затварянето на сцена би убило чужд топъл процес.
- **`finished` → `deleteLater`.** Процесът, който сме стартирали, се трие от
  собствения си сигнал, тоест не остава висяща връзка към мъртъв обект.

### Задачи в общ пул: `CancelToken`, не флаг в нода

`ThreadOwner`/`ProcessOwner` притежават *собствена* нишка/процес. Задача, дадена
на **споделен** пул (`ComputePool`, REQ-SW-PL-039), не се притежава от никого:
`cancel(key)` само **чака** до 500 ms за работещата задача — не може да я убие.
Затова проверката „още ли ме искат?" не бива да живее в нода, защото самата
проверка е достъп до паметта, която може да е освободена.

`Threading/CancelToken.h` дава `std::shared_ptr<CancelToken>`: нодът пази един,
всяка задача взима **копие по стойност** и първото ѝ изречение е
`if (Daqster::isCancelled(token)) return;`. Нодът вика `cancel()` **преди**
`ComputePool::cancel(key)` — след това token-ът (и отговорът) е валиден, дори
когато нодът вече не е.

## Кой какво освобождава (нодовата страна)

| Ресурс | Притежател | Освобождава се в |
|---|---|---|
| зает ресурс на нода (контекст, връзка) | нодът, който го е взел | `stop()`; деструкторът е втората защита |
| споделен ресурс зад брокер | брокерът, не нодът | политиката при затваряне — вижте долу |
| топъл външен процес | нодът, само ако е **собствен** | `stop()` → адаптерът гаси процеса |
| `AudioOutput` sink | публичен `AudioOutputNode` | `stop()` → `AudioPlaybackEngine::stop()` |
| `AudioSource` capture | публичен `AudioSourceDataModel` | `stop()` → `quit()` + `wait()` |

**Преизползван чужд топъл процес не се пипа** — той има свой собственик, а
нодът само затваря връзката си.

### След `stop()` нодът продължава да получава обръщения

`stop()` не е краят на разговора: стъпка 3 трие връзките на възела **преди** да
го унищожи, а `DataFlowGraphModel::deleteNode()` вика `outputConnectionDeleted()`
на производителя за всяка изтрита връзка (`deleteConnection` →
`sendConnectionDeletion`). За `AudioSource` това означаваше
`setCaptureEnabled(false)` → `QMetaObject::invokeMethod` върху worker, който
`stop()` вече е унищожил (`quit()` + `wait()`; `QThread::finished` →
`deleteLater`): use-after-free при `File → Close Scene` (backtrace: кадри 2–7 —
`setCaptureEnabled` → `outputConnectionDeleted` → `sendConnectionDeletion` →
`deleteConnection` → `deleteNode` → `clearScene`).

Затова нод, който държи helper обект в **друга нишка**, спазва три правила:

- пази го с **`QPointer`** и проверява `if (!w) return;` преди **всяко**
  `invokeMethod` (не само преди `setCaptureEnabled` — същото важи за
  `startCapture`/`stopCapture`/`updateDevice`, тоест и за `start()`, и за
  `onUiStart()`, и за `onAudioConnectionChanged()`);
- `stop()` вдига **еднократен флаг** (`m_captureStopped`) и след него всички
  worker-обръщения са no-op — дори pointer-ът да е още жив (QPointer става null
  само при реално унищожаване, а deferred delete може да не е приключил);
- флагът се вдига **преди** `quit()/wait()`, за да покрие и прозореца, в който
  worker-ът е в процес на унищожаване; `stop()` е идемпотентен и не оставя
  pointer към worker-а.

Проверката е `test_thread_lifecycle.cpp` (`stopThenConnectionDeletedIsSafe`,
`stopThenUiAndDeviceCallsAreInert`, `audioSourceSurvivesFiveSceneCloses`): те
скубят освободения блок с `0xAB`, за да не минава use-after-free-ът „на късмет".

### Опасните `stop()` пътища (D1–D4)

Примитивите описват **как** се спира; тези четири поправки премахват
останалите места, на които `stop()` можеше да блокира завинаги или да пипне
освободена памет:

| # | Място | Преди | Сега |
|---|---|---|---|
| D1 | `DaqDisplayNode::stop()`, `VideoEffectNode::stop()` | работната задача проверяваше `m_shuttingDown` — тоест **член на нода**, който `ComputePool::cancel()` (500 ms) може вече да е унищожил | задачата държи `CancelToken` **по стойност** и проверява него преди всяко пипане на `this`; `stop()` отменя token-а **преди** `cancel()` |
| D3 | `PlutoSdrEngine::stop()` | `QThread::wait()` без срок — блокиран `iio_buffer_refill()` спираше затварянето завинаги | `wait(3000)`; при изтичане връща `TimedOut` и **не** вика `terminate()` (убиването на нишката вътре в libiio оставя драйвера в неопределено състояние); `close()` тогава не разрушава iio контекста |
| D4 | `InferenceWorker::stop()` (частно) | `wait()` без срок — зареждане на модел, което не може да се отмени, блокираше изхода на процеса | `wait(5000)`; при изтичане логва и **откача** нишката (разрушаването на работещ `QThread` вдига `abort()`), а `start()` отказва втора |

`PlutoSdrEngine` държеше работната нишка и като гол указател, а тя се триеше
през `finished → deleteLater`: след като event loop-ът на главната нишка
отработеше това, `stop()` викаше `wait()` върху освободен обект. Сега е
`QPointer<QThread>`.

## Какво остава по дизайн

Затварянето не е „освободи всичко". Остава:

- **теглата при `keepWarm`** (по подразбиране) — IDE-то отваря и затваря сцени
  многократно и студеният старт на 8B модел е 15–30 s; `idleUnloadMs` (5 min) ги
  връща, ако сцената не се отвори пак. Политиката е на плъгина
  (`runtime.closePolicy` / `DAQSTER_AI_CLOSE_POLICY`): `keepWarm` | `releaseAll`.
- **`InferenceWorker` нишката** — услуга на процеса, не на сцената (и вече
  спира с краен срок, D4).
- **историята на чата** — runtime състояние, не ресурс.

Тоест след `POST /api/flow/close` с `keepWarm` VRAM-ът **не** пада до базовата
стойност: падат процесите, контекстите и sink-ът. С `releaseAll` пада и VRAM.

## REST

`POST /api/flow/close` → `{ "closed": true }` (200). Идемпотентно: празна или вече
затворена сцена е 200, а не грешка — затварянето се вика и от „зарежи друга
сцена". 409 (`{"closed": false, "error": …}`) само когато host-ът няма canvas
(напр. GUI runtime преди `RunRuntime()`).

## Тестове

`tests/framework/engine/scene_close_tests` (без реален модел, fake node models):

- `sceneCloseStopsNodesAndReleasesContexts` — нула живи контексти след затваряне,
  а плъгинът вижда точно това състояние;
- `doubleCloseIsIdempotent` — второто затваряне не спира, не трие и не уведомява
  никого повторно;
- `closeWithoutSceneIsNoOp` — никога не отваряна сцена е успешен no-op;
- `stopOrderIsSourcesFirstNotInsertionOrder` — верига, строена от консуматора
  назад, се спира от източника напред;
- `cycleIsStillFullyStopped` — цикъл не оставя работещ нод.

Отварянето (`tests/framework/engine/scene_open_tests`, fake providers, без
плъгин и без модели на диска):

- `providerWithoutHookIsSkipped` — доставчик без hook се пропуска и не се брои;
- `oneProviderReportsAllProblems` — един отказ с три причини връща трите, не
  първата, всяка с id, име, съобщение и подсказка;
- `twoProvidersAreUnioned` — двама отказващи: обединението, в ред на доставчиците;
- `problemsAreSerialised` — `problemsJson()` дава `nodeId`/`nodeName`/`message`/
  `hint`, включително празен `nodeId` за проблем за цялата сцена;
- `messageNamesNodeAndHint` — редът на съобщението адресира нода и подсказката,
  а липсващата подсказка не оставя висящо тире;
- `acceptedSceneProducesNoProblems` — приемане = празен списък и празно
  съобщение (хостът няма какво да покаже);
- `noProvidersIsAcceptance` — никой не имплементира hook-а: сцената се отваря,
  както преди hook-а да съществува;
- `providersAskedIsCounted` — `providersAsked` брои питaните, не откритите
  (null и без hook не се броят), а съобщението съдържа проблема и „Nothing was
  loaded";
- `hookReceivesTheScene` — hook-ът получава обекта, който би се заредил (вижда
  нодовете), не празен обект.

Ограничение (записано и в REQ-то): `HeadlessEngine::applySceneJson()` е private,
а публичното repo не носи тестов плъгин — затова **редът** в хостовете
(прочети → питай → спри → зареди) не се покрива от тест; тества се шевът
(`checkSceneOpening()` + fake доставчици).

Примитивите (`tests/plugins/common/`, binary `daqster_common_tests`):

- `ThreadOwner`: `stopBeforeStartIsNoOp`, `invokeDeliversToWorker`,
  `stopTwiceIsIdempotent`, `invokeAfterStopIsNoOp`,
  `stopTimesOutAndTerminates` (worker блокира event loop-а си: `quit()` не се
  чува, `stop()` трябва да прекрати нишката и да върне `TimedOut` до ~4 s);
- `ProcessOwner`: `ownedProcessIsTerminated` (учтив път), `foreignProcessIsNotKilled`
  (`adopt(p, false)` → процесът остава жив), `setOwnedFalseForbidsTheKill`,
  `killFallbackOnStubbornProcess` (фалшив сървър с `trap '' TERM` → `Killed`),
  `stopTwiceIsIdempotent`. Фикстурата е POSIX (`QSKIP` на Windows) и чака
  **handshake файл**, а не фиксирано закъснение — иначе SIGTERM може да пристигне
  преди `trap`-а и тестът „доказва" kill пътя с процес, който нищо не е игнорирал.

Нодовите регресии (`tests/plugins/nodeditor_nodes/`):

- D1: `displayComputeTaskChecksTokenBeforeTheNode` (след `delete node` и
  задраскване на блока с `0xAB` token-ът още отговаря), `cancelledTaskGuardSurvivesTheNode`
  (същото за `VideoEffectNode`);
- D1+сцена: `audioSourceAndDisplaySurviveFiveSceneCloses` — 5 × (AudioSource →
  DaqDisplay, старт, `stopSceneNodes` източниците първи, изтриване на възлите
  под задраскан heap).
