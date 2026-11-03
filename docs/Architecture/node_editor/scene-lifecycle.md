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

## Какво остава по дизайн

Затварянето не е „освободи всичко". Остава:

- **теглата при `keepWarm`** (по подразбиране) — IDE-то отваря и затваря сцени
  многократно и студеният старт на 8B модел е 15–30 s; `idleUnloadMs` (5 min) ги
  връща, ако сцената не се отвори пак. Политиката е на плъгина
  (`runtime.closePolicy` / `DAQSTER_AI_CLOSE_POLICY`): `keepWarm` | `releaseAll`.
- **`InferenceWorker` нишката** — услуга на процеса, не на сцената.
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
