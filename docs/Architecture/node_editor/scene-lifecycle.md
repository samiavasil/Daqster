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

## Кой какво вика

| Път | Какво прави | Код |
|---|---|---|
| IDE `File → Close Scene` (Ctrl+W, бутон в toolbar-а) | пита при нестартирани промени → `stopSceneNodes()` → `notifySceneClosed()` → `clearScene()` | `NodeEditorIdeObject::closeCurrentScene()` |
| IDE `File → Load Scene…` | същото, без въпроса (сцената се подменя) | `NodeEditorIdeObject::loadSceneFromFile()` |
| `NodeRunner --run` (GUI), смяна на сцена | `stopAllNodes()` → `notifySceneClosed()` → `clearScene()` | `RuntimeShell::loadFlow/reloadFlow/closeFlow` |
| `NodeRunner --headless`, `POST /api/flow` | `stopAllNodes()` → `notifySceneClosed()` → нов `DataFlowGraphModel` | `HeadlessEngine::loadFlow/reloadFlow` |
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
  сигнализира — иначе затварянето на сцена би убило чужд топъл `whisper-server`.
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
| `ContextHandle` (KV, `n_ctx`) | нодът, който го е взел (`ChatNode`, LLM нодове) | `stop()`; деструкторът е втората защита |
| тегла (`ModelHandle`) | брокерът (`ModelRuntime`), не нодът | политиката при затваряне — вижте долу |
| топъл `whisper-server` | `AsrNode` → `AsrAdapter` (само **собствен**) | `stop()` → `AsrAdapter::stop()` → `stopServer()` |
| топъл `piper` | `TtsNode` → `TtsAdapter` | `stop()` → `killWarmProcess()` |
| `llama-mtmd-cli` | `VlmNode` → `VlmAdapter` | `stop()` → `VlmAdapter::stop()` (гаси и retire()-натите процеси) |
| `llama-tts` (на заявка) | `TtsAdapter` | `stop()`/деструктор |
| `AudioOutput` sink | публичен `AudioOutputNode` | `stop()` → `AudioPlaybackEngine::stop()` |
| `AudioSource` capture | публичен `AudioSourceDataModel` | `stop()` → `quit()` + `wait()` |

**Преизползван чужд `whisper-server` не се пипа** — той има свой собственик, а
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
| D2 | `LLamaModelDataModel::stopServer()` | `terminate()` + `waitForFinished(3000)`; сървър, който игнорира SIGTERM, оставаше жив, а нодът се унищожаваше | + `kill()` + `waitForFinished(1000)`; връща `NotRunning/Stopped/Killed/TimedOut` и логва при kill |
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
- D2: `llamaModelStopKillsStubbornServer` — реален `QProcess`, който игнорира
  SIGTERM → `Killed` след ~3 s, `NotRunning` при повторение;
- D1+сцена: `audioSourceAndDisplaySurviveFiveSceneCloses` — 5 × (AudioSource →
  DaqDisplay, старт, `stopSceneNodes` източниците първи, изтриване на възлите
  под задраскан heap).
