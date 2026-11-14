# Changelog
Всички значими промени по този проект ще бъдат документирани в този файл.

Форматът следва [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) и
проектът използва [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- **`REQ-SW-PL-092` — DSP ядро за семплирани потоци: полифазен FIR, дециматор, ресемплер, коефициенти като данни** (2026-11-14, клон `feat/REQ-SW-PL-092-dsp-core`):
  Смяната на честотата нямаше къде да живее: всеки нод, който искаше 48 kHz → 16 kHz или 44.1 → 48, щеше да си пише свой филтър, а „какъв филтър точно" щеше да е невидимо — тоест два прогона с еднакви числа можеха да дадат различни резултати без обяснение. Нов `src/plugins/common/Dsp/` в `DaqsterCommonLibrary` (само `Qt::Core`, без GUI/нишки/процеси): **`FirCoefficients`** — импулсен отговор като **данни** с произход: `makeDefault(taps=64, cutoffRatio=0.45, β=8)` (windowed-sinc с Kaiser прозорец, нормализиран до sum = 1), `antiAliasForDecimation(M)`/`antiAliasForResampling(up, down)`, `fromJson()` (документ с `taps`/`file`/`ref`/`sha256`/`delay`/`cutoff`), `fromTapsFile()` (голи числа, по едно на ред, `#` коментар), `validate()` (дължина 8…4096, крайни, DC gain 1 ± 1e-3, съгласуваност на декларираното закъснение), `delaySamples()` = (taps−1)/2 и `sha256OfTaps()` — идентичността, която прави резултата възпроизводим. **`FirDecimator`** (цяло M): филтрира и пази всяка M-та извадка, тоест (taps/M) умножения на изход вместо на вход; **`PolyphaseResampler`** (рационално L/M): `y[i] = Σ_j h[p + j·L]·x[q − j]`, където `q = ⌊i·M/L⌋`, `p = (i·M) mod L` — позицията в потока е **целочислено деление на индекса на изхода**, не натрупана дроб, и точно това прави парчетата еквивалентни на един голям блок (5 × 100 = 1 × 480, бит по бит); прототипът се умножава по L, за да не е промяната на честотата промяна на усилването. И двата са стрийминг с нулево начало (първият изход идва след M·(1+закъснение) входа, не след taps мъртва тишина), `latencySamples()` в **изходни** извадки, `latencyInputSamples()` във входни, `reset()` пази конфигурацията. **`SampledData` получава timebase** (`firstIndex`, −1 = неизвестен, по подразбиране — старите конструкции не се променят), `blockSamples()` **изчислен** от буфера и дескриптора (не съхранен: съхраненото копие е още едно число, което може да се разминава с байтовете), `durationSeconds()` и `isContiguousWith(prev, prevSamples)` — дупка, която не може да се изключи, е дупка. Тестове: `daqster_common_tests` +8 (`TestDspFilters` — нормализация/линейна фаза/стабилен хеш, DC gain 1 ± 1e-3 за двата енджина, **105.8 dB** потискане на 20 kHz→4 kHz при децимация ×3 на 48→16 kHz, амплитуда ±1 % и честота през брояч на пресичанията, бит-идентична детерминираност, закъснение срещу измерен центроид на енергията ±1 извадка, зареждане на JSON/голи числа + откази с числото в текста, еквивалентност на парчетата), `nodeditor_nodes_tests` +1 (`TestSampledData::timebaseIsDerivedAndGapsAreDetected`). Верификация: Qt6 + Qt5 `ctest -R "common|nodeditor"` **21/21** на двата; `demo_nodeditor_core_widgets_free` минава (DSP-то е QtCore). Решението и числата (48 kHz…100 MHz × 8) са в частния `ADR-AI-016`.
- **`REQ-SW-PL-090` — `CMakePresets.json`: един източник на конфигурация за qt5/qt6** (2026-11-13, клон `build/cmake-presets-ninja-ccache`):
  QtCreator и `scripts/build.sh` пишеха в един и същ `build_qtX/`, но с различни настройки — кешовете бяха **Unix Makefiles** + `/usr/bin/c++`, а IDE-то налагаше **Ninja** + `/usr/lib/ccache/…` компилатори, тоест всеки втори билд беше „всичко наново". Сега конфигурацията е файл: `qt6-debug` → `build_qt6/`, `qt5-debug` → `build_qt5/` (Ninja, `CMAKE_C_COMPILER=/usr/bin/cc`, `CMAKE_CXX_COMPILER=/usr/bin/c++`, `CMAKE_CXX_COMPILER_LAUNCHER=ccache`, `CMAKE_EXPORT_COMPILE_COMMANDS=ON`, `DAQSTER_BUILD_TESTS=ON` + всяка `DAQSTER_*` опция, която кешовете вече носеха), плюс `buildPresets` със същите имена. Реалните ускорения остават: ccache е launcher, не компилатор, тоест пътят на компилатора е един и същ за двата входа. `scripts/build.sh` мина на `cmake --preset <p>` + `cmake --build`; стойност, която се разминава с пресета (`BUILD_TYPE=Release`, `BUILD_TESTS=OFF`), отива в `build_qtX-custom/` — споделеният dir е запазен за пресета. Машинни пътища се слагат в `CMakeUserPresets.json` (gitignored). Проверката, че живият кеш съвпада с пресета, е `scripts/check-build-config.sh` в частното repo (ниво 0 на `test-level.sh`). Тестовете вече са **включени по подразбиране** — превключването на `DAQSTER_BUILD_TESTS` при всяко пускане беше втората причина за препълване на кеша.
- **`REQ-SW-PL-091` — относителните пътища в сцената се решават спрямо сцената** (2026-11-12, клон `feat/REQ-SW-PL-091-scene-variables`):
  Токенът реши само половината от преносимостта: сцена, която носи файл **до себе си** (`"filePath": "bars_h265_1080p25_x3.mp4"`), продължаваше да подава относителния низ на `QUrl::fromLocalFile`/`QFile`, тоест той се решаваше спрямо **cwd на процеса** — вярно само ако процесът е стартиран в директорията на сцената (затова тестовете залагаха машинен път). `SceneVariables` получава двойка ambient състояние + резолвер: `setSceneDirectory()`/`sceneDirectory()` (нормализирана лексикално, под същия mutex, документирана като ambient — един процес пуска една сцена), `adoptSceneFile(<път до .flow>)` — **единственият** helper, който трите хоста викат (`HeadlessEngine::applySceneJson()` преди `recordSceneOpening()`, `RuntimeShell::applySceneJson()`, `NodeEditorIdeObject::loadSceneFromFile()`), и `resolvePath(raw, token*, reason*)`: токени както досега → абсолютният резултат непокътнат → относителният закачен за `sceneDirectory()` и нормализиран (`..` се сгъва; файловата система не се пита) → без сцена **празно + причина**, никога cwd. `adoptSceneFile("")` (REST тяло, `reloadFlow(QJsonObject)`) **оставя** директорията: hot-reload не бива да обръща собствените си пътища на cwd-относителни. `VideoFileSourceNode`/`FilePlaybackModel` минават на `resolvePath()` и различават трите дефекта в `NodeValidationState::Error` (неразрешен `${X}` / относителен път без сцена / липсващ файл); `FilePlaybackModel::loadFile()` резолвва и път, въведен в widget-а. За да стигне резолверът до framework-а, `DaqsterCommonLibrary` се свързва към `FrameworkCore` (едната посока без цикъл — общата библиотека не зависи от нищо проектно; декларирано в `src/plugins/common/CMakeLists.txt`, защото `src/frame_work` се конфигурира първи). Тестове: `daqster_common_tests` +2 (`TestSceneVariables::resolvePathAnchorsRelativePathsToTheScene`, `resolvePathWithoutASceneDirectoryReportsWhy`), `demo_nodeditor_lifecycle_tests` +1 (`relativePathsResolveAgainstTheSceneFile` — сцена с относителен `filePath` се зарежда през `DataFlowGraphModel` и файлът се намира) и разширен `sceneVariablesAreExpandedOnLoad` (липсващ файл като трети случай). Документация: `docs/plugins/nodeditor_nodes/README.md` → „Относителните пътища в сцената се решават спрямо директорията на сцената".
- **`REQ-SW-PL-091` — `${VAR}` резолвер за сцени, разширяван от нода при зареждане** (2026-11-12, клон `feat/REQ-SW-PL-091-scene-variables`):
  `.flow` в git не можеше да носи абсолютен път, а два публични нода подаваха литерала направо на `QUrl::fromLocalFile`/`QFile` — тоест сцена с `${DAQSTER_DEPS}` работеше само при експортирана променлива (скриптовете я заобикаляха с временно копие). Нов `src/plugins/common/Scene/SceneVariables.{h,cpp}` в `DaqsterCommonLibrary` (`Qt::Core`, namespace `Daqster`): `expandDetailed(raw, overrides)`/`expand(raw, token*)` с частния `JsonConfig::expandEnv` като референтен синтаксис (`${X}`, `${X:-d}`, `${X:?m}`, `$$`; без `$VAR` и без рекурсия) и предимство **override → конфиг на машината (група `paths` в `<config>/Daqster/scene-variables.json`) → среда → празно**. `${X:?m}` връща празно и докладва токена + съобщението; непознат `${X}` остава литерал (видим в GUI-то). Персистентност: `setPathToken()`/`removePathToken()`/`pathTokens()` (атомарно през `QSaveFile`, другите ключове на документа оцеляват) и `seedFromEnvironment()`, което копира **само липсващи** ключове. `defaultFilePath()` предпочита launcher-независимия `~/.config/Daqster/…` (децата на IDE-то получават `XDG_CONFIG_HOME=~/.config/daqster`, тоест `GenericConfigLocation` сам не стига); `DAQSTER_SCENE_VARIABLES` е тестов/нестандартен override. `VideoFileSourceNode::load()` и `FilePlaybackModel::load()` разширяват `filePath` при зареждане и дават `NodeValidationState::Error` с името на токена, вместо късен „Invalid media"/„Cannot open"; `*Ref`/`namedRef` не се пипат. Тестове: `daqster_common_tests` +9 (`TestSceneVariables` — синтаксис, предимство, `:?`, `$$`, persist кръг, отказ на имена, seed без презапис, един корен на машина) във временен `XDG_CONFIG_HOME` + `HOME`; `demo_nodeditor_lifecycle_tests` +1 (`sceneVariablesAreExpandedOnLoad` — двата нода). Документация: `docs/plugins/nodeditor_nodes/README.md` → „`${VAR}` в сцените (REQ-SW-PL-091)".
- **`REQ-SW-PL-090` — дисплейни нодове за текст, картина, кадров прозорец и embedding** (2026-11-10, клон `feat/REQ-SW-PL-090-node-debug-displays`):
  Графът нямаше как да покаже какво всъщност пренася — `TextData`, `ImageData` и `FrameWindowData` стигаха само до консуматор, който човекът пише сам, тоест при диагностика се гледаше лог вместо входа на нода. Три нови **дисплея-консуматори** (`Displays/General`, 1 вход / 0 изхода, `embeddedWidget()` = `nullptr`, само `const` достъп до входа) в `nodeditor_nodes/core/Displays/`: **`TextDisplayNode`** (`TextData`) — последният текст (превю до 240 знака, цялото в tooltip) + брой блокове, знаци, редове и „преди N ms"; **`ImageDisplayNode`** (`ImageData`) — превю, смалено до 320×180 **на worker-а** (пълната картина никога не се задържа: снапшотът носи копие, не буфера на източника) + ширина×височина на източника, формат на пикселите, mime/model/elapsed от `Meta`; **`FrameWindowDisplayNode`** (`FrameWindowData`) — брой кадри, timestamps на първия/последния (+ разлика) и до 4 миниатюри през `VideoFrameData::frameToImageScaled()` (чист CPU, безопасен от всяка нишка). GPU-резидентен кадър **не** се конвертира (`asImage()` е GL readback само за GUI нишката) — показва се „metadata only — N frame(s) not CPU-convertible" вместо измислена миниатюра. Общата 30 Hz хореография (QTimer + per-node `ComputePool` ключ + `CancelToken`, REQ-SW-PL-039/050) е изнесена веднъж в `Displays/DisplayRefresh/DisplayRefreshDriver`; тялото на всеки дисплей е в GUI плъгина и се вписва в `NodeWidgetFactory` като съседите. `NumberDisplayDataModel` е **разширен**, не дублиран: трети режим `embedding` (`dim · min · max · mean` + първите 8 компоненти, `dataType()` следва селектора, `save()`/`load()` пазят `"embedding"`), а числовите пътища `double`/`int` остават байт-идентични. Тестове: `nodeditor_nodes_tests` +21 (`TestDisplayNodes` — консуматорски договор за трите, броячи/отрязване/редове на текста, доставен снапшот през pool-а, метаданни и ограничено превю на картината + не-алиасинг + „нодът не мутира payload-а", метаданни и ≤4 миниатюри на прозореца + metadata-only за GPU кадри, статистика/порт/сериализация на embedding-а, непроменено `double` поведение). Документация: `docs/plugins/nodeditor_nodes/README.md` (таблица на дисплейното семейство + правилото „дисплеите са консуматори: 1 вход, 0 изхода, без мутация").
- **`REQ-SW-PL-088` — декларативно управление на външни зависимости (`DependencyManager`)** (2026-11-10, клон `feat/REQ-SW-PL-088-dependency-manager`):
  Външните артефакти се въвеждаха ръчно на пет места (env променливи, `servers.json`, `model-catalog.json`, подразбирания в скриптовете и твърди `--check` списъци), без checksum и без версия. Нов `src/plugins/common/Dependencies/DependencyManager.{h,cpp}` в `DaqsterCommonLibrary` (`Qt::Core` + `Qt::Network`, namespace `Daqster`): `DependencySpec` (`id`, `kind` = `binary|library|model|voice|source`, `version`, `url`, `sha256`, `size`, `file`, `provides`, `executable`) и `DependencyManager` с `loadManifest()` (all-or-nothing валидация на `dependencies.json`), `status()` (файл + размер + sha256, по lockfile пътя за adopt-нати файлове), `install()` (async през `QNetworkAccessManager` → `QSaveFile` → sha256 **в движение** → `commit()`, тоест атомарно и без partial файл; един download на id), `adopt()` (проверява и записва абсолютния път в lockfile-а **без копиране**) и `lock()`/`lockFilePath()` (`deps.lock.json`: `{id,version,path,sha256,adopted}`); сигнали `progress(id,received,total)` и `finished(id,ok,error)`. Правила: **само HTTPS** (`http://`/`file://` отказват с ясен текст, `NoLessSafeRedirectPolicy`), sha256 **задължителен** за теглене, без shell-out и без `waitFor*()`, `..`/`\`/`:` в `id`/`version`/`file` се отхвърлят. Тестове: `daqster_common_tests` +15 (`manifestParses`, `manifestRejectsNonHttps`, `manifestRejectsMissingChecksum`, `manifestRejectsDuplicateIdAndUnknownKind`, `manifestRejectsMalformedSha256`, `statusAcceptsCorrectChecksum`, `statusDetectsChecksumMismatch`, `statusMissingNamesId`, `adoptRecordsLockWithoutCopying`, `adoptRejectsChecksumMismatch`, `adoptOnlyRecordsComputedChecksum`, `installRejectsNonHttps`, `installRejectsMissingChecksum`, `lockRoundTripAcrossInstances`) — без мрежа, HTTPS-ът се покрива през `adopt()` и валидационната половина на `install()`. As-built: `docs/Architecture/dependency-manager.md`.
- **`REQ-SW-PL-087` — WebSocket клиент във framework-а (`WebSocketClient`)** (2026-11-10, клон `feat/REQ-SW-PL-087-websocket-client`):
  Репото имаше ръкописен WebSocket **сървър** в контролната плоскост и **никакъв клиент** — всеки консуматор щеше да си пише свой, а лесният изход (`Qt::WebSockets`) е отделен Qt модул, който липсва при много инсталации. Нов `src/plugins/common/WebSocket/WebSocketClient.{h,cpp}` в `DaqsterCommonLibrary`, само `Qt::Network` (`QTcpSocket`/`QSslSocket`), namespace `Daqster`: `open(QUrl)` (само `ws`/`wss`; `wss` през `QSslSocket` и **само** при `supportsSsl()`, иначе ясен отказ вместо тих plain текст), `close(code = 1000)`, `isOpen()`, `sendText()`/`sendBinary()` (всеки клиентски кадър е **маскиран**, RFC 6455 §5.3), сигнали `connected`/`disconnected(code, reason)`/`textMessage`/`binaryMessage`/`errorOccurred`. RFC 6455: HTTP Upgrade с `Sec-WebSocket-Key` и **валидация** на `Sec-WebSocket-Accept`, стрийминг парсер с фрагментация (continuation), ping→pong, close handshake с код/причина и 1.5 s грация, протоколна грешка при RSV/маскиран сървърен кадър, `maxMessageSize` (8 MiB, 1009 при превишаване), `handshakeTimeoutMs` (10 s) — **без нито един `waitFor*()`**, само сигнали на сокета. Тестове: `daqster_common_tests` +10 (`handshakeValidatesAccept`, `clientFramesAreMasked`, `textRoundTrip`, `binaryRoundTrip`, `fragmentedMessageIsReassembled`, `pingIsAnsweredWithPong`, `closeCodeRoundTrip`, `invalidAcceptIsRejected`, `oversizedMessageIsRejected`, `wssIsRefusedWithoutTls`) с мини WebSocket сървър `QTcpServer` в самия тест. Верификация: Qt6 + Qt5 `BUILD_TESTS=ON`, `ctest -j4` **32/32** на двата. As-built: `docs/Architecture/websocket-client.md` (+ `.en.md`).
- **`REQ-SW-PL-086` — входно семплиране: прозорец от кадри като data тип + нод (`FrameWindowData` / `FrameWindowNode`)** (2026-11-09, клон `feat/REQ-SW-PL-086-frame-window`):
  Omni/vision нодът трябваше сам да събира кадри, за да получи контекст от няколко последователни кадъра — тоест всеки консуматор повтаряше един и същ буфер. Нов **header-only** `src/plugins/common/NodeDataTypes/FrameWindowData.h` (`QtNodes::NodeData`, `type()` = `{"frame-window", "Frame Window"}`) носи `QVector<Item>` с `Item { std::shared_ptr<VideoFrameData> frame; qint64 timestampMs; }`, `items()`, `isEmpty()`, `count()`, `firstTimestampMs()`, `lastTimestampMs()`; копирането е изтрито (преносът е по `shared_ptr`), пиксели не се копират — всеки Item държи **същия** `VideoFrameData`. Нов `FrameWindowNode` (`Processing/Video`): вход port 0 `VideoFrameData`, изход port 0 `FrameWindowData`; sliding прозорец от последните `window` кадъра (2…32, default 4) с arrival timestamps и три режима — `full` (emit при напълване, после на всеки нов кадър), `everyFrame` (всеки кадър, и частичен прозорец), `interval` (най-много веднъж на `intervalMs`, default 1000; първият кадър веднага). Null/невидео вход изчиства прозореца и emit-ва `dataInvalidated(0)`. `save()`/`load()` пазят `emitMode`/`window`/`intervalMs`. Widget-free модел (REQ-SW-PL-051) + `FrameWindowWidget` в GUI плъгина (mode combo + два QSpinBox-а); регистриран и в двата списъка на `NodeditorNodesCoreObject`. Тестове: `nodeditor_nodes_tests` +11 (`portsAndDataTypes`, `embeddedWidgetIsNull`, `optOutOfGeometryRecompute`, `nameAndCaption`, `fullModeWaitsForWindowThenEmitsEveryFrame`, `everyFrameModeEmitsOnEveryFrame`, `intervalModeEmitsAtMostOncePerInterval`, `windowKeepsOnlyLastFrames`, `windowSizeIsClamped`, `itemsShareTheInputSharedPointers` — същият `constBits()`, `nullInputClearsWindowAndInvalidates`, `nonVideoInputClearsWindow`, `saveLoadRoundTrip`). Документация: `docs/plugins/nodeditor_nodes/README.md`, `docs/Architecture/node_editor/nodes.md`.
- **`REQ-SW-PL-085` — плъгин може да предложи „Поправи“ за проблем на сцената** (2026-11-09, клон `feat/REQ-SW-PL-085-problem-fixer`):
  `ISceneOpening` (REQ-SW-PL-082) казваше *какво* не е наред, но поправката оставаше ръчна: дори когато плъгинът знае точното действие („липсва модел — изтегли го“, „смени backend-а“), човекът трябваше да го изведе от `hint`-а. Нов header-only capability **`IProblemFixer`** (`src/plugins/common/capabilities/IProblemFixer.h`; plain non-`QObject`, включва `ISceneOpening.h` за `SceneOpenProblem`) с `canFix(problem)`, `fixLabel(problem)` (празен = без бутон) и `fix(problem, QString* error)`; **хостът не поправя нищо** — пита, показва и вика. Откриване по модела на `IMenuProvider`: `PluginRegistry::problemFixers()` / `QPluginManager::problemFixers()` с `dynamic_cast`, връща инициализирани обекти, питат се няколко и първият `canFix` печели. `SceneProblemsPanel` получава `setFixHandler(label, fix)` и слага **„Поправи“** на реда, за който има fixer (`fixButtonAt()`, `statusText()` за тестове); провалът се показва в **собствения статус ред на панела** (без модален диалог, редът остава за повторен опит), а успехът излъчва `fixRequested(problem)` — **отложено** с една итерация, защото хостът изчиства списъка и би изтрил бутона в собствения му `clicked()`. IDE-то (`NodeEditorIdeObject::adoptProblemFixers()`, `onProblemFixed()`) подава handler-а, маха реда и маркировката на нода и препрочита валидационните състояния. Тестове: `node_editor_ide_tests` +4 (`problemWithoutFixerHasNoButton`, `fixerShowsLabelAndIsInvoked`, `fixFailureIsReportedInPanel`, `panelRefreshesAfterFix`). Документация: `docs/Architecture/capabilities.md` §`IProblemFixer`, `docs/Architecture/node_editor/scene-lifecycle.md` → „Поправи“ до проблема.
- **`REQ-SW-PL-082` — пътят до сцената стига до hook-а (`onSceneOpening(scene, scenePath)`)** (2026-11-08, клон `feat/REQ-SW-PL-082-hook-scene-path`):
  Плъгин, който валидира сцена, често трябва файл **до** нея — `project.json` с пътищата на моделите за този проект, include директория, относителни ресурси — а от JSON-а не може да се разбере къде стои файлът. Новият default-implemented overload `ISceneOpening::onSceneOpening(const QJsonObject&, const QString& scenePath)` носи пътя на `.flow` файла; препраща към едноаргументния, тоест плъгин, компилиран срещу стария header, работи непроменен (нов virtual **не се добавя** към `ISceneLifecycle`, vtable-ът на `ISceneOpening` се разширява само с default — затова е overload, а не сменен подпис). Празен `scenePath` = сцена без файл (`reloadFlow(QJsonObject)`, REST тяло на `POST /api/flow`), **не** „текуща директория". `SceneOpen.{h,cpp}` получава `checkSceneOpening(scene, scenePath[, providers])`; старите две продължават да съществуват и делегират с празен път. Хостовете подават пътя, който вече имат: `HeadlessEngine::loadFlow()` → `applySceneJson(sceneJson, flowPath)` → `recordSceneOpening()` (при `reloadFlow(QJsonObject)` — празен), `RuntimeShell::loadFlow()` → `applySceneJson(sceneJson, fileName, …)`, `NodeEditorIdeObject::loadSceneFromFile(fileName)`. Тестове: `scene_open_tests` +3 (`scenePathReachesTheHook`, `oldProviderStillWorks`, `emptyPathIsAllowed`). As-built: `docs/Architecture/capabilities.md` §`ISceneOpening`, `docs/Architecture/node_editor/scene-lifecycle.md`.
- **`REQ-SW-PL-084` — capability за действия в менюто от плъгин (`IMenuProvider`)** (2026-11-08, клон `feat/REQ-SW-PL-084-plugin-menu-provider`):
  Плъгин можеше да сложи действие само в `Tools → Plugins`, защото `IStatusBarProvider` (REQ-SW-PL-079) е договор за **състояние**, а единственото му меню е фиксирано; „`Tools → AI Setup`" е действие на Tools, не ред в статус бара, и хостът не можеше да го приеме, без да знае за плъгина. Нов header-only `src/plugins/common/capabilities/IMenuProvider.h`: plain (non-`QObject`, без QtWidgets) интерфейс с `PluginMenuAction { menuPath, text, id, enabled }` (`menuPath` е `/`-разделен път на заглавия), `menuActions()` и `triggerMenuAction(id)`. Discovery: `PluginRegistry::menuProviders()` / `QPluginManager::menuProviders()` — `dynamic_cast` като при другите plain capability-та, връща **инициализирани** обекти (действието работи с онова, което `Initialize()` е построило). IDE-то инсталира действията през новия `PluginMenuController` (`src/plugins/node_editor/node_editor_ide/PluginMenuController.{h,cpp}`, `NodeEditorIdeObject::installPluginMenuActions()`): сегментите се резолвват по заглавие (`&` игнориран), съществуващо меню/подменю се **преизползва**, липсващото се създава; `setProviders()` е идемпотентен (маха само своите действия и само създадените от него менюта), празен `id`/`menuPath` се пропуска с предупреждение, изключение от провайдъра се логва и не се разпространява. Тестове: `node_editor_ide_tests` +7 (`actionsAppearUnderRequestedPath`, `submenuIsCreatedOnlyOnce`, `triggerSendsTheProviderId`, `installingTwiceDoesNotDuplicate`, `disabledAndInvalidEntries`, `providerThatThrowsIsSkipped`, `noProvidersMeansNoActions`) с фалшив провайдър — без плъгин. Документация: `docs/Architecture/capabilities.md` §`IMenuProvider`, `docs/Architecture/node_editor/README.md`.
- **`REQ-SW-PL-080` — публичен `ImageData` (единично изображение, не видео кадър)** (2026-11-06, клон `feat/REQ-SW-PL-080-image-data`):
  Платформата имаше тип за видео кадър, но не и за **една картинка** — тоест генерирано изображение не можеше да стигне до публичен дисплей/запис и `.flow`, който смесва публични и частни нодове, не можеше да свърже изхода. Нов header-only `src/plugins/common/NodeDataTypes/ImageData.h`: `QtNodes::NodeData` с `type()` = `{"image", "Image"}`, payload `QImage` + `Meta` (`prompt`, `negativePrompt`, `seed`, `model`, `mimeType`, `elapsedMs`), `const QImage &image()`, `const Meta &meta()`, `isEmpty()` (`QImage::isNull()`) и `width()`/`height()` — **от `QImage`**, не копирани в `Meta` (иначе две истини за един факт). Копирането е изтрито (както при `VideoFrameData`): преносът е по `std::shared_ptr`, а по стойност би detach-нал `QImage`-а и би започнал да дублира пиксели — точно дефектът на премахнатия предшественик. Зависимости само `QImage` (QtGui) + QtNodes — **без QtWidgets/Multimedia**, тоест типът работи headless. Тестове: `tests/plugins/nodeditor_nodes/test_image_data.{h,cpp}` в `nodeditor_nodes_tests` (7 случая) — `typeIdIsImage`, `sharedPtrTransferKeepsTheSameBuffer` (еднакъв `constBits()` след преноса), `metadataSurvivesTransfer`, `defaultConstructedIsEmpty`, `imageFromNullImageIsEmpty`, `repeatedReadsDoNotDetach` (`isDetached() == false` след четения), `widthHeightComeFromTheImage`. Верификация: Qt6 + Qt5 `BUILD_TESTS=ON` — 0 error / 0 warning, `ctest -j4` **32/32** на двата. Документация: `docs/plugins/nodeditor_nodes/README.md` („единственият **поточен** frame тип"), `DevelopmentProcess/AGENT-KNOWLEDGE.md` (редът „`ImageData` е ПРЕМАХНАТ" е поправен).
- **`REQ-SW-PL-081` — конфигурируеми флагове за host/port в `LocalServerService`** (2026-11-06, клон `fix/REQ-SW-PL-081-configurable-port-flags`):
  Класът винаги добавяше `--host <h> --port <n>`, а `sd-server` приема само `--listen-ip/--listen-port` и излиза при непознат аргумент — тоест сървър с друга конвенция не можеше да се пусне през framework примитива. Ново поле `LocalServerSpec::portFlags` (по подразбиране `{"--host","--port"}`; първи = host, втори = port) се ползва в `launch()` и при четенето на порта от `/proc/<pid>/cmdline` (orphan sweep), вместо литералите. `start()` отказва синхронно и с ясен текст: `portFlags`, които не са празни и не са точно два различни непразни елемента, и `arguments`, които вече съдържат избраните флагове (или `--host`/`--port`) — за да не се подаде портът два пъти. Тестовият stub вече е strict (непознат аргумент → exit 2) като реален сървър и приема и двете двойки флагове. Тестове: `test_local_server_service` +3 (`customPortFlagsAreUsed`, `invalidPortFlagsAreRejected`, `defaultFlagsUnchanged`). As-built: `docs/Architecture/local-server-service.md`.
- **`REQ-SW-PL-082` — IDE: живите валидационни грешки в панела с проблеми („червено без текст")** (2026-11-06, клон `fix/REQ-SW-PL-082-live-node-validation-panel`, от PL-081 клона):
  Нод в `NodeValidationState::Error` се рисуваше червен, но съобщението му беше само **tooltip**, а иконката за проблема се рисува извън нода в `nodeeditor` submodule-а — тоест потребителят виждаше „червено без текст". Панелът `SceneProblemsPanel` показваше само присъдата на `ISceneOpening` **при отваряне**, така че грешка, появила се след това (сменен backend в widget-а, изчезнал файл), не се виждаше никъде. Сега нов `NodeEditorIdeObject::refreshNodeProblems()` обхожда `allNodeIds()`, чете `NodeRole::ValidationState` (+`Caption`) и подава състоянията на новите `NodeProblems::{liveProblems,merge,sameProblems}` заедно с пазената присъда (`m_sceneOpenProblems`). Правила: ред за всяко състояние ≠ `Valid` (и `Warning`), празно съобщение получава резервен текст; дедупликация по (`nodeId`, `message`) — маркираният от `applySceneProblems()` нод носи същия текст и се вижда веднъж, при две копия оцелява това с `hint`; щом нодът стане `Valid`, редът изчезва и празен списък скрива панела. Обновяване по сигналите на графа (`nodeUpdated`/`nodeCreated`/`nodeDeleted`/`modelReset`), събрани в single-shot `QTimer` (0 ms) + `sameProblems()` — без polling и без презареждане на панела всеки кадър; таймерът се спира при затваряне. Тестове: `node_editor_ide_tests` +3 (`liveValidationErrorsAppearInPanel`, `panelDeduplicatesSameProblem`, `panelClearsWhenNodeBecomesValid`). As-built: `docs/Architecture/node_editor/scene-lifecycle.md` → „Живи валидационни грешки (не само при отваряне)"; tooltip-ът на канваса и мястото на иконката са известен дефект на submodule-а и **не** се пипат тук.

- **`REQ-SW-PL-082` — IDE: панел с проблеми и маркиране на нодовете („зарежда, маркира, не стартира" и в редактора)** (2026-11-06, клон `feat/REQ-SW-PL-082-scene-diagnostics`):
  IDE-то беше последният хост с **вето**: `File → Load Scene…` показваше `QMessageBox::warning` и не зареждаше нищо, тоест при един липсващ модел човекът оставаше на празен канвас — а графът е точно мястото, на което пътят се поправя. Сега `NodeEditorIdeObject::loadSceneFromFile()` пита `checkSceneOpening()` **след** успешния `load()` (нодовете трябва да съществуват, за да бъдат маркирани) и `applySceneProblems()`: за всеки проблем с непразен `nodeId` нодът получава `setValidationState({State::Error, message — hint})` (рисува се върху тялото на нода), по един `qCWarning(lcNodeEditor)` на проблем, и всички отиват в новия **`SceneProblemsPanel`** под канваса — хедър `Проблеми в сцената (N)`, свит по подразбиране, редове `<nodeName> (<nodeId>): <message> — <hint>`; двойно щракване или Enter излъчва `problemActivated(nodeId)` и IDE-то селектира нода и центрира изгледа върху него (`clearSelection` + `setSelected` + `centerOn`, с проверки за null). Празен списък **скрива** панела, затварянето на сцената го изчиства, модален диалог няма — сцената остава отворена за редактиране. Тестове: `node_editor_ide_tests` +6 (`emptyProblemsHidePanel`, `problemsFillListAndStoreIds`, `activationEmitsNodeId`, `problemTextIncludesHint`, `enterKeyActivatesSelectedRow`, `panelStartsCollapsedAndExpands`). As-built: `docs/Architecture/node_editor/scene-lifecycle.md` → „IDE-то: маркирай, не отказвай".

- **`REQ-SW-PL-082` — сцената се зарежда с проблеми, стартът чака („зарежда, маркира, не стартира")** (2026-11-06, клон `feat/REQ-SW-PL-082-scene-diagnostics`, от PL-082 диагностика клона):
  Присъдата на `ISceneOpening` беше **вето**: един липсващ модел връщаше човека към празен екран, тоест отказваше му и графа, който е поискал — а точно графът е мястото, на което се поправя проблемът. Сега `HeadlessEngine` и `RuntimeShell` **винаги зареждат** сцената, пазят присъдата в `m_lastSceneProblems` и само **не стартират** нодовете с `autoStart` (`recordSceneOpening()` в двата host-а, вика се на последната врата — в `applySceneJson()`, след филтрирането на непознатите типове, тоест върху сцената, която реално се зарежда). Проблемите се логват по един `qCWarning` на проблем и се съобщават през `errorOccurred()`/`sceneOpeningMessage()`. Нов `RuntimeController::lastSceneProblems()` (non-pure virtual, по подразбиране `{}`) дава присъдата на control plane-а, а `POST /api/flow` вече отговаря с **двете** неща: `{"applied": true, "started": <problems.isEmpty()>, "problems": [{nodeId,nodeName,message,hint}, …]}`; `400 {"applied": false, "error": …}` остава само за документ, който не може да се зареди изобщо. `POST /api/flow/close` и `/save` не са променяни. IDE `File → Load Scene…` още отказва — отделна задача. Тестове: `control_plane_router_tests` +2 (`reloadFlowReportsProblemsWithoutStarting`, `reloadFlowCleanSceneStarts`) през fake `RuntimeController` — без плъгин и без модел; `scene_open_tests` не е променян (там се тества само helper-ът, а host-редът е private без публичен плъгин за тест). `ctest` Qt6 **37/37**. As-built: `docs/Architecture/node_editor/scene-lifecycle.md` → „Отварянето е договор" + `docs/Architecture/capabilities.md` §`ISceneOpening`.

- **`REQ-SW-PL-082` — структурирани проблеми при отваряне на сцена (`SceneOpenProblem`)** (2026-11-06, клон `feat/REQ-SW-PL-082-scene-diagnostics`, от PL-082 hook клона):
  `SceneOpenVerdict` носеше `QStringList problems`, тоест отказът можеше да каже *какво* не е наред, но не и *кой нод* — хостът можеше само да отпечата списък и да остави човека да търси нода в графа. Сега присъдата носи `QVector<SceneOpenProblem>`: `nodeId` (`nodes[].id`, може празен за проблем за цялата сцена), `nodeName`, `message` и `hint` (какво да направи човекът), а `accepted()` остава `problems.isEmpty()`, тоест празната присъда е приемане както преди. `SceneOpenReport` (`src/frame_work/base/src/engine/SceneOpen.{h,cpp}`) получава същия `QVector<SceneOpenProblem> problems`, `problemsJson()` (`nodeId`/`nodeName`/`message`/`hint` — за REST тяло или хост, който маркира нодове) и `sceneOpeningMessage()`, който рендира по един ред на проблем: `<nodeName> (<nodeId>): <message> — <hint>`, с пропуснати празни части. Поведението на трите хоста не се променя в този комит — те само продължават да компилират срещу новия тип и да отказват с текста; маркирането на нодове е следващата задача. Тестове: `scene_open_tests` +2 (`problemsAreSerialised`, `messageNamesNodeAndHint`), съществуващите случаи проверяват id/име/подсказка. As-built: `docs/Architecture/capabilities.md` §`ISceneOpening` + `docs/Architecture/node_editor/scene-lifecycle.md` → „Отварянето е договор".

- **`REQ-SW-PL-082` — `ISceneOpening`: отварянето пита плъгините преди `load()`** (2026-11-04, клон `feat/REQ-SW-PL-082-scene-opening-hook`, от PL-081 клона):
  `ISceneLifecycle` покриваше само затварянето, а „тази сцена не може да работи тук" (липсващ модел, непознат backend, модел, който не се побира в бюджета) се разбираше едва **след** `load()` — тоест след като старата сцена вече е спряна и унищожена. Нов optional capability **`ISceneOpening`** (`src/plugins/common/capabilities/ISceneOpening.h`; plain non-`QObject`, само QtCore типове) връща `SceneOpenVerdict{problems}` — **всички** причини, не първата, а празен списък значи приема. **Защо не слот в `ISceneLifecycle`:** той няма `Q_DECLARE_INTERFACE` IID и версия, тоест плъгин, компилиран със стария header, би се викал през слот извън своя vtable (класът дефекти INC-014); затова `ISceneLifecycle.h` и `INodeProvider.h` остават **байт-идентични**, а `DAQSTER_PLUGIN_INTERFACE_*` не се пипа (`QPluginInterface.h:254-258` — all-or-nothing, би счупил всеки несвързан плъгин). Реализацията е една — `FrameworkCore::SceneOpen` (`src/frame_work/base/src/engine/SceneOpen.{h,cpp}`: `SceneOpenReport`, `checkSceneOpening()` + overload през `QPluginManager`, `sceneOpeningMessage()` по модела на `unsupportedHeadlessMessage()`), а hook-ът е задължен да отговаря **без** `Initialize()` (`PluginRegistry.cpp:232-239` умишлено не вика `ensureInitialized()` — иначе един въпрос би построил цял `RuntimeShell`) и да не стартира процеси/нишки/модели.
  **Редът е поправен (в обхвата на REQ-то):** и двата non-IDE host-а спираха старата сцена и унищожаваха графа ѝ **преди** да прочетат файла — невалиден път или скъсан JSON убиваше работеща сцена, без да зареди нищо. Сега: прочети → питай → (при отказ: нищо) → спри старото → зареди новото. `HeadlessEngine::loadFlow()` (`HeadlessEngine.cpp:121` четене, `:128` питане, `:135-138` спиране) и `reloadFlow()` (`:177`), `RuntimeShell::loadFlow()` (`RuntimeShell.cpp:322` → `:334`) и `reloadFlow()` (`:1316`); `NodeEditorIdeObject::loadSceneFromFile()` пита след филтрирането и **преди** `closeCurrentScene()` (`:801` → `:815`) и при отказ показва `QMessageBox::warning` със списъка, без да пипа текущата сцена. Последна врата преди `load()`/`clearScene()` има и в `HeadlessEngine::applySceneJson()` (`:463`) и `RuntimeShell::applySceneJson()` (`:435`), а отказът през `POST /api/flow` излиза като 400 с текста на проблема (`ControlPlaneRouter.cpp:224-230`). Close контрактът (REQ-SW-PL-078) не е променян: `notifySceneClosed(Replace)` е на същото място спрямо разглобяването, тоест `keepWarm` политиката на REQ-AI-021 работи както преди.
  **Тестове:** нов `tests/framework/engine/scene_open_tests` (`QTEST_GUILESS_MAIN`, fake providers — без плъгин и без модели): `providerWithoutHookIsSkipped`, `oneProviderReportsAllProblems`, `twoProvidersAreUnioned`, `acceptedSceneProducesNoProblems`, `noProvidersIsAcceptance`, `providersAskedIsCounted`, `hookReceivesTheScene`. Публичен `ctest` **33 → 34** (Qt6, 0 error / 0 warning). As-built: `docs/Architecture/capabilities.md` §`ISceneOpening` + `docs/Architecture/node_editor/scene-lifecycle.md` → „Отварянето е договор".

- **`REQ-SW-PL-079` — статус бар за плъгините (`IStatusBarProvider`)** (2026-11-03, клон `feat/REQ-SW-PL-079-status-bar-provider`, от PL-050 клона):
  Плъгин, който държи глобално състояние (заредени тегла, заета VRAM, топъл inference сървър), нямаше къде да го покаже: то не е на нито един нод, значи никой node widget не може да го изобрази, и единственото, което потребителят виждаше, беше `nvidia-smi` — където „топъл модел, готов за преизползване" изглежда точно като изтекла памет. Нов capability **`IStatusBarProvider`** (`src/plugins/common/capabilities/IStatusBarProvider.h`; plain non-`QObject`, само QtCore типове, без `QWidget`) дава на плъгина `statusId()`/`statusText()`/`statusTooltip()`/`statusOrder()`, списък `Action`-и и `triggerStatusAction(id)`; интерфейсът е **генеричен** — нищо в него не е за AI или VRAM. **Откриване:** `PluginRegistry::statusBarProviders()` (`dynamic_cast`, като при `IWidgetProvider`) + делегация в `QPluginManager`; обектите се връщат **инициализирани**, защото редът показва живото състояние на онова, което `Initialize()` е построил. **IDE:** нов `StatusBarController` в `node_editor_ide` — по един permanent `QLabel` на провайдър (временното съобщение не ги скрива), подредени по `statusOrder()` и после по `statusId()`; tooltip при hover **и** при клик, а действията са в `Tools → Plugins` (клик върху ред умишлено не изпълнява действие — то освобождава ресурси). **Обновяване:** `QTimer` 2 s, изрично след `Load Scene…` и `Close Scene`, при промяна на списъка плъгини и веднага след изпълнено действие. **Устойчивост:** null провайдър, празен `statusText()` (редът се скрива), хвърлящ `statusText()`/`triggerStatusAction()` (лог, скрит ред, без crash) и убит plugin object (`QObject::destroyed` → редът се маха, хостът преоткрива) не чупят IDE-то. **Тестове:** нов `status_bar_discovery_tests` (фалшив plugin интерфейс — без `.so`, без диск) + 7 случая в `node_editor_ide_tests` (offscreen: текст, действие, скрит ред, липсващ/хвърлящ/убит провайдър, ред на подредба, дубликат, скрито празно меню). As-built: `docs/Architecture/plugins-status-bar.md`. Имплементацията от AI плъгина (топли модели + заета VRAM) е следваща стъпка в частното repo.
- **`REQ-SW-PL-078` — причина за затварянето (`SceneCloseReason`): hot-reload ≠ изричен close** (2026-11-03, клон `feat/REQ-SW-PL-078-close-reason`):
  `ISceneLifecycle::onSceneClosed()` казваше *че* сцената изчезва, но не и *защо* — плъгин, който пази ресурси след нодовете си (кеширани тегла, топъл сървър), не можеше да различи hot-reload от `File → Close Scene`. Новият `Daqster::SceneCloseReason` (`Replace` / `ExplicitClose`) минава по цялата верига — `notifySceneClosed()`, `stopAndNotifySceneClosed()`, `closeSceneGraph()` го приемат, `SceneCloseReport::reason` го връща — а `HeadlessEngine` (`loadFlow`/`reloadFlow`/`closeFlow`), `RuntimeShell` и `NodeEditorIdeObject` подават истинската причина. **Причината е информация, не заповед:** хостът казва *защо*, а какво остава в паметта решава плъгинът и неговата настройка (env/settings/UI) печели пред нея — затова нито един път не освобождава кеш автоматично. Старият хук `onSceneClosed()` остава отделен virtual слот, който новият вика по подразбиране, тоест плъгин без причина продължава да работи. Тестове: `explicitCloseReasonReachesPlugin`, `replaceReasonReachesPlugin`, `legacyProviderIsStillNotifiedWithReason`.
- **`REQ-SW-PL-050` — миграцията към примитивите е завършена; правилата се налагат статично** (2026-11-03, клон `refactor/REQ-SW-PL-050-thread-process-owner`): `AudioSourceDataModel`/`MicCaptureWorker`, `PlutoSdrEngine` и `PcapEngine` вече нямат собствен `QThread` — `ThreadOwner` носи latch-а, join-а с краен срок и worker-а; Pluto ползва `stop(..., terminateOnTimeout=false)` + `detach()`, защото убиване вътре в `iio_buffer_refill()` оставя libiio в неопределено състояние. `LLamaModelDataModel::stopServer()` минава през `ProcessOwner` (terminate → 3 s → kill → 1 s; същият D2 тест). Правилата 1–7 + 10-редов пример са в `docs/Architecture/threading.md`; нов `tests/plugins/nodeditor_nodes/check_no_bare_threads.cmake` (ctest `demo_nodeditor_no_bare_threads`) сканира `src/plugins/**` без `common/Threading/` за `new QThread`, `QThread::create`, `class : QThread` и `new QProcess` и **пада при инжекция**. Qt5 + Qt6 `BUILD_TESTS=ON` — 0 warning; `ctest` **29/29** (+1 check).
- **`REQ-SW-PL-050` — два примитива за lifetime (`ThreadOwner`, `ProcessOwner`) + поправени опасни `stop()` пътища** (2026-11-03, клон `feat/REQ-SW-PL-078-scene-close`):
  Спирането на фонова работа се пишеше на ръка във всеки нод и всяко копие бъркаше една от четирите стъпки (`quit()` без `wait()`, `wait()` без краен срок, pointer към worker, който нишката вече е изтрила). Правилата вече са на едно място, в `src/plugins/common/Threading/`:
  **`ThreadOwner<Worker>`** (header-only шаблон) притежава `QThread`-а **и** worker `QObject`-а: `start(factory)` създава worker-а, `moveToThread()`, свързва `finished → deleteLater`; `stop(deadlineMs = 3000, terminateMs = 500)` вдига latch-а **преди** `quit()` (затова `invoke()`/`alive()` отказват работа на умираща нишка), чака с краен срок, при нужда `terminate()` и връща `NotRunning`/`Stopped`/`TimedOut` — никога безкрайно чакане; след успешен `stop()` worker pointer-ът е null. **`ProcessOwner`** (`ProcessOwner.{h,cpp}`, в `DaqsterCommonLibrary`) притежава един `QProcess`: `terminate()` → `waitForFinished(grace)` → `kill()` → `waitForFinished(killWait)` → `NotRunning`/`Stopped`/`Killed`/`TimedOut`; `setOwned(false)`/`adopt(p, false)` за преизползван (чужд) процес, тоест `stop()` го **забравя**, вместо да го убие; `finished → deleteLater` за процесите, които сам е стартирал. **Защо два примитива:** нишката се *присъединява*, а процесът се *моли да излезе* и може да откаже — един клас би носил две несъвместими значения на „спиране" плюс същия флаг за убиване, само че скрити. **`Threading/CancelToken.h`** покрива третия случай — задача в **споделения** `ComputePool`, която никой не притежава и `cancel()` не може да убие: нодът пази `shared_ptr<CancelToken>`, всяка задача го взима **по стойност** и проверява `Daqster::isCancelled(token)` преди да пипне `this`, а `stop()` го отменя **преди** `ComputePool::cancel()`.
  **Поправки по съществуващите stop пътища (D1–D4):** **D1** `DaqDisplayNode::stop()` / `VideoEffectNode::stop()` — задачата в пула се пазеше с `m_shuttingDown`, **член на нода**, тоест самата проверка четеше паметта, която 500 ms след `cancel()` може вече да е освободена; сега проверява token-а по стойност (`displayComputeTaskChecksTokenBeforeTheNode`, `cancelledTaskGuardSurvivesTheNode`). **D2** `LLamaModelDataModel::stop()` (`stopServer()`) — `terminate()` + 3 s беше целият разказ, тоест сървър, който игнорира SIGTERM, оставаше жив, а нодът се унищожаваше сочейки го; сега `kill()` + 1 s, `StopResult` и лог ред (`llamaModelStopKillsStubbornServer`). **D3** `PlutoSdrEngine::stop()` — `QThread::wait()` без срок (блокиран `iio_buffer_refill()` спираше затварянето завинаги); сега `wait(3000)` → `TimedOut`, без `terminate()` (убиването на нишката вътре в libiio оставя драйвера в неопределено състояние) и `close()` тогава пази iio контекста; worker pointer-ът вече е `QPointer<QThread>`, защото се трие през `finished → deleteLater` и гол указател висеше веднага щом event loop-ът отработи. **D4** (частното repo) `InferenceWorker::stop()` — `wait(5000)` + лог, а нишката се **откача**, вместо да се разруши, когато не се прибере (разрушаването на работещ `QThread` вдига `abort()`).
  **Тестове:** нов `tests/plugins/common/` → `daqster_common_tests` (9 `ProcessOwner` + 5 `ThreadOwner` случая, `QCoreApplication`, сорсовете се компилират директно в бинарника), `audioSourceAndDisplaySurviveFiveSceneCloses` (5 × отваряне → пускане → `stopSceneNodes` → изтриване, под задраскан heap), D2 регресия с реален `QProcess`. Миграцията на останалите нодове към примитивите е отделна стъпка. Qt5 + Qt6 `BUILD_TESTS=ON` — 0 error / 0 warning, `ctest` **29/29** на двата. As-built: `docs/Architecture/node_editor/scene-lifecycle.md` → „Два примитива за lifetime".
- **`REQ-SW-PL-078` — затваряне на сцена: `File → Close Scene` + `POST /api/flow/close`** (2026-11-02, клон `feat/REQ-SW-PL-078-scene-close`, от PL-077 клона):
  „Отвори сцена" съществуваше от началото, „затвори" — не: единственият начин да се освободи сцена беше да се затвори целият IDE (или процесът), а при подмяна на сцена (`File → Load Scene…`) `clearScene()` триеше моделите, без да им е викнат `stop()`. Сега затварянето е **договор** с три стъпки и един ред: `IStoppable::stop()` на всеки нод (**източниците първи** — топологично по връзките, детерминистично по id; граф с цикъл се спира докрай), после `ISceneLifecycle::onSceneClosed()` на всеки `INodeProvider`, който го имплементира (нов optional capability, `capabilities/ISceneLifecycle.h`), и едва тогава `clearScene()`/`deleteNode()` унищожава моделите. Реализацията е **една** — нов `FrameworkCore::SceneClose` (`src/frame_work/base/src/engine/SceneClose.{h,cpp}`: `sceneStopOrder()`, `stopSceneNodes()`, `notifySceneClosed()`, `closeSceneGraph()`, `clearGraphNodes()`, `SceneCloseReport`) — и се ползва от трите host-а, за да не се разминават. **UI:** `File → Close Scene` (`QKeySequence::Close` = Ctrl+W) **и** бутон **Close Scene** в нов toolbar до Save/Load; пита „Затвори без запазване?" само когато има наблюдавани промени (dirty се вдига от сигналите на graph model — `nodeCreated/Deleted/Updated/PositionUpdated`, `connectionCreated/Deleted` — и се нулира при load/save), при празна сцена е no-op. **REST:** `POST /api/flow/close` → `{"closed": true}` (200, идемпотентно; 409 само когато host-ът няма canvas), `RuntimeController::closeFlow()` е част от интерфейса и от OpenAPI документа, web UI-то на control plane-а получи бутон „Close flow". **Идемпотентност:** второ затваряне не спира, не трие и **не уведомява** плъгините повторно — иначе `releaseAll` би освободил топъл модел при затваряне, което не е променило нищо. **Тестове (без реален модел):** нов `tests/framework/engine/scene_close_tests` — `sceneCloseStopsNodesAndReleasesContexts`, `doubleCloseIsIdempotent`, `closeWithoutSceneIsNoOp`, `stopOrderIsSourcesFirstNotInsertionOrder`, `cycleIsStillFullyStopped`; `control_plane_router_tests` + `closesFlow` (200/409/405). **Верификация:** Qt5 + Qt6 `BUILD_TESTS=ON` — 0 error / 0 warning; `ctest` **28/28** на двата (+1 нов binary). As-built: `docs/Architecture/node_editor/scene-lifecycle.md` + `capabilities.md` §`ISceneLifecycle`.
- **`REQ-SW-PL-077` — `AudioSinkNode` (WAV запис) се мести в публичното: `Sinks/Audio`** (2026-10-29, клон `feat/REQ-SW-PL-077-audio-sink-to-public`, от PL-076 клона):
  Нодът не е AI: не ползва **нито един** AI тип, само `SampledData`, `QFile` и (за наследеното `playbackEnabled`) аудио устройството — но всеки, който искаше да запише аудио от граф, трябваше да зареди целия AI плъгин с неговите модели. Сега е `src/plugins/node_editor/nodeditor_nodes/core/Sinks/Audio/AudioSinkNode.{h,cpp}` до `AudioOutputNode`, категория `Sinks/Audio`, в **двата** списъка (`registerNodes()` и `registerNodesHeadless()`), с GUI widget (`gui/Sinks/Audio/AudioSinkWidget.*`, регистриран в `NodeWidgetFactory`) по стила на `AudioOutputWidget`. **`name()` остава `"AudioSinkNode"`**, `caption()` — `"Audio sink"`: идентичността в `.flow` е по `model-name` (PL-057), тоест старите сцени се зареждат без миграция, а `save()` пази същите ключове (`outputPath`, `playbackEnabled`, `volume`). `playbackEnabled` е запазен за съвместимост, но е **deprecated** и делегира на един дългоживеещ `AudioPlaybackEngine` (същия като при `AudioOutput`), вместо да създава `QAudioSink` на блок; GUI-то го показва с tooltip „deprecated — use the AudioOutput node". `${VAR}` разширяването на `outputPath` е портнато локално (`${X}`, `${X:-default}`, `${X:?msg}`, `$$`) — `.flow` файловете остават преносими. **Поправен дефект (AC4):** старият `writeWav()` отваряше файла с `QIODevice::Truncate` на **всеки** блок и презаписваше header-а — при 2+ блока във файла оставаше само последният. Сега файлът се отваря веднъж (truncate при първия блок на нов запис, никога дозапис към чужд файл), header-ът се пише с нулеви размери и двата размера (`RIFF.ChunkSize`, `data.Subchunk2Size`) се патчват на място след всеки блок — файлът е валиден WAV по всяко време, не само след `stop()`. Тестове: `demo_nodeditor_audiosink_tests` (16, вкл. `threeBlocksAreAllInOneValidWav` — 3 блока → 3-те са във файла, header-ът е валиден) + `demo_nodeditor_audiosinkwidget_tests` (6, offscreen); `ctest` **24/24** на Qt5 и Qt6, `node_catalog` — `AudioSinkNode` в `Sinks/Audio` **точно веднъж**.
- **REQ-SW-PL-076 — `AudioOutput`: публичен нод, който свири на изходното устройство** (2026-10-29, клон `feat/REQ-SW-PL-076-audio-output-node`, от `develop`):
  Досега единственият начин да чуеш аудио от граф беше WAV + външен плеър — публичното repo нямаше **нито един** `QAudioSink`. Новият нод `AudioOutput` (`Sinks/Audio`) държи **един** дългоживеещ `AudioCompat::AudioOutput` (`QAudioSink` на Qt6 / `QAudioOutput` на Qt5) за целия живот на нода, избира устройство по **стабилен** `deviceId` (не по индекс), пита `isFormatSupported()` преди да пише и при отказ минава на `preferredFormat()` + ресемплинг/микс/прекодиране (`AudioFormatBridge.h`, `AudioResample.h`) — никога тих no-op. Backpressure през `bytesFree()`: пълната опашка е `bufferMs` (20–2000 ms) и блокове се **пропускат** с лог, вместо да расте буфер или да блокира графа. Без изходно устройство (headless, CI) нодът е no-op със статус, а `IStartable`/`IStoppable` пази «не свири, докато графът само се редактира» (PL-050). Модел/изглед по PL-053: `AudioOutputNode` е без QtWidgets, `AudioOutputWidget` (QToolBox „Output", combo по `deviceId()`, volume, `bufferMs`, `enabled`, статус) идва от GUI плъгина през `NodeWidgetFactory`. `AudioCompat` се разширява с `AudioOutput`, `defaultOutputDevice()`, `deviceId()`, `deviceFromId()`. Всички потребителски текстове (статус, причини, логове) са на английски, както е останалият публичен UI. Тестове: `demo_nodeditor_audiooutput_tests` (16, без изискване за звукова карта) + `demo_nodeditor_audiowidget_tests` (7, offscreen); `ctest` **22/22** на Qt5 и Qt6, `node_catalog` — 50 типа, `Sinks/Audio/AudioOutput`.
- **REQ-SW-PL-073 — plugin mode gating: headless не зарежда GUI плъгини** (2026-10-17, клон `feat/REQ-SW-PL-073-plugin-mode-gating`, от PL-072 клона):
  Launcher-ът (PL-072) извади QtWidgets от **бинарника**, но `QPluginManager::SearchForPlugins()` продължаваше да зарежда **всеки** намерен плъгин — вкл. GUI-само, които връщаха widget-стека в headless процеса. Сега плъгинът декларира режима си с `PluginType` (`gui` — GUI приложения/runtime host; `NodeGui` — доставчици на widget-и за нодове; `headless`; липсва = режим-неутрален), а manager-ът чете стойността през `QPluginLoader::metaData()` — **без** `dlopen` — и в `RuntimeMode::Headless` пропуска `gui`/`NodeGui`. Нов `PluginDiscovery::pluginType()` (резолвва и трите срещани формата на метаданните), `QPluginManager::SetRuntimeMode()`/`GetRuntimeMode()`, `AppBootstrap::initializePlugins(mode)`; `NodeRunner` подава режима, `Daqster` не (→ GUI зарежда всичко както преди). Маркирани: `RuntimeHostGuiPlugin`, `NodeEditorIDE`, `RequirementsManager`, `QtCoinTrader` = `gui`; `NodeditorNodesGui` = `NodeGui` (частният `AiStudioPluginGui` също `NodeGui`). **Резултат:** headless зарежда само `RuntimeHostHeadlessPlugin` + `NodeditorNodesCorePlugin` — на **Qt6** с **0× `libQt6Widgets`** (истински widget-free). На **Qt5** `libQt5Widgets` остава, но **не** от наши плъгини: `NodeditorNodesCorePlugin` линква `Qt5Multimedia`, чиито gstreamer `mediaservice` плъгини hard-link-ват `Qt5MultimediaWidgets` (Qt5 ограничение; Qt6 `Qt6Multimedia`/ffmpeg няма такава зависимост). Верификация: Qt5 **20/20** + Qt6 **20/20** ctest; GUI режим зарежда всичките 7 плъгина (RSS ≈177 MB) срещу 3 headless (≈104 MB).
- **REQ-SW-PL-059 — визуален Layout Designer в NodeEditor IDE** (2026-10-15, клон `feat/REQ-SW-PL-059-layout-designer`):
  Редакторът вече строи schema-v2 `LayoutSection` (split/grid/tab/panel + docks) с мишката, вместо ръчно редактиране на `.flow` JSON. Нов **Layout Designer Mode** (F6, чекваемо меню действие) скрива canvas-а и показва `LayoutDesignerPanel`: toolbar ([Split H/V] [Grid 2×2] [Tab] [Dock ◀▶▲▼] [+WS/−WS] [Clear Slot] [Capture] [Undo/Redo] [Save Layout] [Exit]), ляв панел **Available Widgets** (drag source с търсене), среден **Structure** дърво (drop targets за всеки root/контейнер/grid-клетка/tab-страница/dock) и десен **live preview**, построен с `LayoutEngine::buildWorkspace()`. `m_section` е единственият източник на истината — всяка мутация пази undo снимка и препостроява дървото + preview-то; `Capture` прави round-trip през `LayoutEngine::captureWorkspace()`. При вход нодовете с widget се deembed-ват (както F11), при изход се re-embed-ват; preview-то освобождава graph-owned виджетите преди изтриване, за да не се унищожат моделите.
  `NodeEditorIdeObject`: при `ui.version >= 2` load-ът диспатчва към `LayoutSection::fromJson` и пази секцията за дизайнера, иначе остава v1 `UiSection` пътят; save пише `LayoutSection::toJson()` (version 2) когато има v2 секция, иначе legacy `captureUiSection()`. Qt5 + Qt6 билд; нов `TestLayoutDesigner` в `node_editor_ide_tests` (11 случая, вкл. MIME round-trip; `TestLayoutEngine` — 11) — 55/55 зелени на двата Qt-а.

- **REQ-SW-PL-059 — Layout Designer: йерархично влагане, конфигурируем grid и Delete** (2026-10-15, клон `feat/REQ-SW-PL-059-layout-designer`):
  Layout Designer-ът вече строи **произволно вложени** дървета и целият toolbar действа върху **селектирания** възел (не само върху root-а). Данните: grid-ът спря да пази плосък `std::vector<std::optional<NodeId>> cells` и ползва същия `std::vector<LayoutNode> children` (row-major, точно `rows*cols`) като split/tab — така всяка клетка/пейн/страница може да е контейнер (grid в split пейн, tab в grid клетка, …). `LayoutNode::fromJson()` разчита и legacy `"cells"` (`[{node:N}|null]`) и обвива всяка клетка в `panel`, значи стари `.flow` файлове се зареждат; writer-ът винаги пише `children`. `LayoutEngine::buildGrid`/`captureGrid` обхождат `children` рекурсивно. Designer: бутоните `Split H/V`, `Tab`, `Grid` се прилагат върху селектирания слот (split/tab → нов пейн/страница; grid → първа свободна клетка; panel → replace, като съществуващ нод се запазва в първия слот); `Grid` вече е с `rows × cols` spinbox-ове (1–8, default 2×2 — напр. 2×3 за шест дисплея). Нов бутон **`Delete`** маха селектирания компонент (или dock); пуснатите widget-и се **detach-ват** (връщат се в „Available Widgets" — те принадлежат на графа, не на layout-а), а празен split/tab root колапсира до празен панел; всичко минава през `applyMutation` → `Undo`/`Redo`. Секцията е добавена и в `docs/Architecture/formats/flow-format.md` §2.3. Нови тестове: `TestLayoutTree::legacyGridCellsParse`/`nestedGridRoundTrip`, `TestLayoutDesigner::{addGridRespectsRequestedSize,nestedContainerTargetsSelectedSlot,deleteRemovesComponentAndDetachesNode,deleteCollapsesEmptySplitRoot}`; поправени dangling-reference бъгове в тестовете (`panel.layoutSection().workspaces.front()...`). Qt5 + Qt6 build, `node_editor_ide_tests` **61/61** зелени на двата Qt-а.

- **REQ-SW-PL-053/REQ-SW-PL-059 — v2 layout в runtime (`RuntimeShell`)** (2026-10-15, клон `feat/REQ-SW-PL-059-layout-designer`):
  `RuntimeShell` вече строи schema-v2 `ui` секция при `--run`, вместо да я игнорира. `LayoutTree.*`/`LayoutEngine.*` се преместиха от `NodeEditorIde` в споделената **`NodeEditorLibrary`** (единичен източник за дизайнера и runtime-а), а `LayoutEngine` получи `buildInto(QMainWindow*, workspace, provider)` — строи централния контейнер и docks-овете директно върху **вече съществуващ** прозорец (вместо да създава втори `QMainWindow`); `buildWorkspace()` е тънка обвивка върху него. `RuntimeShell::applySceneJson` чете `ui.version`: `>= 2` → `LayoutSection::fromJson` + `arrangeLayoutV2()`, иначе legacy `UiSection` + `arrangeWorkspaces()`. `arrangeLayoutV2()` deembed-ва всеки рефериран нод при поискване през graph provider-а (canvas-ът остава скрит), подава го на `LayoutEngine::buildInto` и ре-стартира autoStart нодовете от v2 `nodes` картата; `flowJson()` round-trip-ва v2 секцията. Qt5 + Qt6 пълен билд, 19/19 ctest. **Ограничения** (документирани): runtime строи първия workspace при няколко дефинирани; REST-създадени нодове във v2 режим остават невидими (няма MDI, `deembedNewNode` е no-op).

- **REQ-SW-PL-054 — REST control plane, първи срязък** (2026-10-11, клон `feat/REQ-SW-PL-054-rest-control-plane`):
  `FrameworkCore` вече носи преносим **HTTP/1.1 сървър върху `QTcpServer`** (не `QHttpServer`, защото Qt5 няма такъв — една реализация за двата Qt-а): `src/frame_work/base/src/control/ControlPlaneServer.{h,cpp}` + widget-free интерфейс `RuntimeController.h`. `HeadlessEngine` имплементира `RuntimeController` и `RuntimeHostHeadlessPluginObject::configureControlPlane()` вдига сървъра от `--rest-*` опциите. Реализирани endpoint-и: `GET /api/health`, `GET /api/nodes` (id/type/name/state), `GET /api/nodes/{id}/config`, `POST /api/nodes/{id}/config` (in-place `NodeDelegateModel::load()`, с rollback при грешка), `POST /api/nodes/{id}/start|stop` (през `IStartable`/`IStoppable`), `GET /api/nodes/{id}/schema` (JSON Schema Draft 7 от `save()` на празен модел), `GET /api/flow` (nodes+connections+ui), `GET /api/openapi.json`. Auth: `Authorization: Bearer <token>` с constant-time сравнение; **fail-closed** — non-loopback bind без token се отказва (exit 1). `HeadlessEngine` вече следи per-node `running/stopped` състояние (`m_runningNodes`). Qt5 + Qt6 билд; smoke тест на двата Qt-а (health/nodes/config/schema/start/stop/auth/404).
  **Довършено 2026-10-11 (втори срязък, `d9aac98`):** `POST /api/flow` hot-reload (AC9 — пълен flow обект или `{"path": ...}`, с типова валидация и `400` при лош документ), WebSocket `/api/events` (AC10 — RFC 6455 върху същия `QTcpServer`/порт, събития `nodeStarted`/`nodeStopped`/`nodeConfigChanged`/`nodeError` като `{type,data}`, `?token=` за браузъри), CORS + `OPTIONS` preflight (AC12), пълен OpenAPI 3.0.3 спек + Swagger UI `/api/docs` (AC13), вграден web console на `/` + статични файлове от `--rest-web-root`, `scripts/fetch-swagger-ui.sh` за локално vendored Swagger UI. Auth вече пази само данните: `/api/docs`, `/api/openapi.json` и статичният UI са достъпни без токен.
  **GUI режим (2026-10-11, трети срязък, `317fe82`):** `RuntimeShell` вече имплементира `Daqster::RuntimeController` и държи `ControlPlaneServer`, така че `NodeRunner --run <flow> --rest-port N` сервира **същия** API като headless (преди `RuntimeHostGuiPluginObject` наследяваше default-ния `configureControlPlane()` → `false` и стартът се проваляше с „did not enable it"). Контролерът достъпва графа lazy, защото bootstrap-ът вдига сървъра преди `RunRuntime()` да построи canvas-а; `POST /api/flow` стопва стария граф, rebuild-ва MDI workspaces и ре-стартира autoStart нодовете; `flowJson()` round-trip-ва UI секцията през `FlowUi::UiSection::toJson()`. Извлечен е споделен `NodeSchemaBuilder.h`, за да дават headless и GUI идентичен `/api/nodes/{id}/schema`.
  **Остават** (следващи срезове): `dataPreview`, mTLS, RBAC/rate-limit (v2). (Автоматични тестове за router-а са добавени в REQ-SW-PL-063.)
- **REQ-SW-PL-063 — пълен контрол на графа през REST + WebSocket** (2026-10-12, клон `feat/REQ-SW-PL-063-full-graph-control`, дете на REQ-SW-PL-054):
  Всеки нод и целият flow вече се управляват отдалечено с паритет на GUI. Извлечен е transport-независим `ControlPlaneRouter` (`src/frame_work/base/src/control/ControlPlaneRouter.{h,cpp}`) — чист `route(method,path,body,bodyValid) → {handled,status,body}`, който се ползва **и** от HTTP транспорта, **и** от новия WebSocket RPC (`{"id","method","path","body"}` → `{"id","status","body"}` на `/api/events`), така че API-то е дефинирано веднъж и е тестваемо без сокет. `RuntimeController` е разширен с `nodeTypes`, `connections`, `nodeDetail`, `createNode`, `removeNode`, `setNodePosition`, `setNodeLabel`, `addConnection`, `removeConnection`, `saveFlow`, `typeSchema`; имплементирани са в **двата** хоста (`HeadlessEngine` — вкл. widget-free; GUI `RuntimeShell` — новосъздадените нодове се deembed-ват в MDI workspace). Нови endpoint-и: `GET /api/node-types`, `POST /api/nodes` (create), `GET/PATCH/DELETE /api/nodes/{id}` (detail/move+label/delete), `GET/POST/DELETE /api/connections` (POST минава `connectionPossible()`), `POST /api/flow/save`. `GET /api/nodes/{id}` вече връща superset (`config`+`schema`+`position`+`label`+`ports`+`state`). Обогатена схема през нова widget-free способност `Daqster::INodeConfigSchema` (`plugins/common/capabilities/INodeConfigSchema.h`, dynamic_cast с fallback към досегашната `save()`-производна схема) — пилот `VideoEffectNode` (enum за `effect`, min/max/default, `visibleWhen`); `NodeSchemaBuilder.h` получава 3-аргументен merge. WebSocket вече е двупосочен; нови събития `nodeCreated`/`nodeDeleted`/`nodeMoved`/`connectionCreated`/`connectionDeleted`. Вграденият web console (AC14) е преписан: create/delete, таблица с връзки + **connect форма** (from node:port → to node:port, портовете се пълнят от node detail) и disconnect по връзка, **schema-генерирана** конфигурационна форма (enum→select, range→number, `visibleWhen` скрива неуместните полета, raw-JSON fallback), Save flow, live лог. Нов headless unit тест `tests/framework/control/` — 26 случая върху `ControlPlaneRouter::route()` и schema merge-а (`-DDAQSTER_BUILD_TESTS=ON`). Qt5 + Qt6 билд; 19/19 ctest; smoke в headless и GUI режим (REST + WS RPC). REST мутациите **заобикалят** `QUndoStack` (документиран non-goal).

### Changed
- **`REQ-SW-PL-091` — конфигурация през API казва дали иска рестарт; `PATCH /api/nodes/{id}` приема `config`** (2026-11-12, клон `feat/REQ-SW-PL-091-scene-variables`):
  `POST /api/nodes/{id}/config` викаше `model->load()` и отговаряше само `{"applied":true}` — тоест клиентът не можеше да разбере, че при **стартиран** нод записът само сменя какво ще прочете следващият `start()`, а работещият нод продължава със старата конфигурация (никакъв намек нито в отговора, нито в OpenAPI). Освен това `PATCH /api/nodes/{id}` приемаше само `position`/`label`/`labelVisible`, тоест „премести и пренастрой" искаше две заявки. Сега `ControlPlaneRouter` строи един отговор за конфигурационен запис — `{"applied", "started", "restartRequired"}` (`ControlPlaneRouter::configMutationResult()`; `started` идва от същото поле `state`, което `GET /api/nodes` вече връща, тоест двата хоста отговарят еднакво без нов virtual в `RuntimeController`); `restartRequired == started`. **Нищо не се рестартира само** — отговорът казва какво да се направи (`stop` + `start`). `PATCH /api/nodes/{id}` приема незадължителен `config` обект и го прилага през **същия** `setNodeConfig` (една имплементация, не втора); отхвърлен `config` връща `400`, **без** да е преместил/преименувал нода (config-ът се прилага пръв), а `PATCH` само с `position`/`label` работи както досега (`restartRequired:false`). OpenAPI: нов `NodeConfigResult` (`/config` и `PATCH`, плюс `400` на `PATCH`) и `PatchNodeRequest.config`; `docs/Architecture/rest-api-remote-control.md` → двете секции. Тестове: `control_plane_router_tests` +7 (`configWriteReportsRestartNeed`, `configWriteRejectionReportsNotApplied`, `patchesConfigThroughTheSameSeam`, `patchConfigAndPositionTogether`, `patchRejectedConfigLeavesPositionAlone`, `patchPositionOnlyStaysBackwardCompatible`, `patchRejectsNonObjectConfig`) и нов **`control_plane_rest_e2e_tests`** — истински `NodeRunner --headless --rest-port` на свободен порт, самообезпечена сцена в самия тест (`NumberSource → NumberResult` + `SystemMonitor`), `POST /api/flow` → `GET /api/nodes` → `PATCH config` (спрян нод: `restartRequired:false`) → `POST .../start` → `PATCH` (стартиран: `restartRequired:true`) → `POST /api/flow/close`, с убиване на процеса при всяко излизане; пуска се само с `DAQSTER_REST_E2E=1` (иначе `QSKIP`), за да не тежи на нормалния `ctest`.
- **REQ-SW-PL-081 — `LocalServerService` се мести във framework-а** (2026-11-04, клон `refactor/REQ-SW-PL-081-local-server-service-to-framework`):
  Животът на локален сървър по HTTP (свободен порт → старт → `GET /health` до готовност → файл за собственост → ограничен стоп) не съдържа нито един ред, специфичен за предметна област, но живееше в частния AI плъгин — тоест всеки друг плъгин с локален сървър щеше да го копира. Сега е `Daqster::LocalServerService` в **`src/plugins/common/Servers/LocalServerService.{h,cpp}`** (пространство `Daqster`, `DAQSTERCOMMON_EXPORT`, `.cpp` в `DaqsterCommonLibrary`, `Qt::Network` в зависимостите на компонента) — до `Threading/`, чиито `ProcessOwner` вече ползва. Поведението е пренесено, не преписвано: `State{Stopped,Starting,Ready,Failed}`, двете състояния на собственост (`own-runner` / `external` преизползван), почистване на сираци през `/proc` (`Q_OS_UNIX`), проба с `setTransferTimeout(800)`, повторни опити на следващия порт (`kMaxStartAttempts = 3`), а `/health` се чака **без** `waitFor*`. Променени са само пространството от имена, пътят на файла за собственост (`<GenericDataLocation>/Daqster/local-servers.json`, беше под името на частния плъгин) и езикът на логовете (`daqster.threading`, английски — публичният код). Тестовете и stub-ът са в `tests/plugins/common/` (осем случая срещу истински stub процес: преизползване без втори процес, `stop()` не гаси чужд, липсващ/неизпълним файл, crash след готовност, `stop()` преди `exec`, идемпотентност, избор на порт), а as-built документацията е `docs/Architecture/local-server-service.md`. Правилото е записано в `docs/Architecture/README.md`: основен framework капацитет се предоставя **оттук**, плъгините го композират. **Верификация:** Qt6 build 0 error/0 warning, `ctest` **31/31**; частно ниво 1 PASS (private 34 · public 31 · GUI 97).
- **REQ-SW-PL-048 — споделен widget-free `AppBootstrap` в `FrameworkCore`** (2026-10-11, клон `refactor/REQ-SW-PL-048-shared-app-bootstrap`):
  `NodeRunner` и `Daqster` дублираха една и съща стартова хореография (logging init → `ShutdownHandler` → `QPluginManager::Initialize()`/`SearchForPlugins()` → resolve на runtime host → event loop → подредено teardown). Тя вече живее веднъж в `Daqster::AppBootstrap` (`src/frame_work/base/src/app/`), който е **widget-free** (FrameworkCore линква само QtCore): не строи application обект, а приема вече конструиран `QCoreApplication&`, така че работи и от headless binary без QtWidgets, и от GUI binary с `QApplication`. `NodeRunner` е преписан върху него; `Daqster/main.cpp` ползва същия bootstrap за споделената част (логиране, shutdown handler, plugin init, teardown), а app-specific логиката остава в `main.cpp`. Новият bootstrap носи и `--rest-port`/`--rest-bind`/`--rest-token`/`--rest-web-root` (REQ-SW-PL-054) и ги подава към хоста през `IRuntimeHost::configureControlPlane()` — хост без remote control кара старта да се провали шумно, вместо тихо да подсказва, че REST сървърът работи.
- **REQ-SW-PL-060 — node-provider конвенция `<plugin>/{core,gui}` + преименуване на shared support библиотеката** (2026-10-09, клон `feat/REQ-SW-PL-056-headless-all-node-components`):
  - `src/plugins/node_editor/nodeditor_nodes_core/` + `src/plugins/node_editor/nodeditor_nodes_gui/` се сливат в **един** плъгин `src/plugins/node_editor/nodeditor_nodes/{core,gui}/` — документирана framework конвенция (`docs/Architecture/core-gui-split.md`, `docs/Architecture/plugins/PluginDevelopment.md`): `core/` е headless (без `QWidget`), `gui/` държи widget-ите и `IWidgetProvider`.
  - Класове/target-и/OUTPUT_NAME: `DemoNodeEditorNodesCore*` → `NodeditorNodesCore*`, `DemoNodeEditorNodesGui*` → `NodeditorNodesGui*` (`libNodeditorNodesCorePlugin.so` / `libNodeditorNodesGuiPlugin.so`). Обновени са plugin `Name` в JSON и `Dependencies` в `RuntimeHostHeadlessPlugin`/`RuntimeHostGuiPlugin`.
  - `docs/plugins/demo_nodeditor_nodes/` → `docs/plugins/nodeditor_nodes/`.
- **REQ-SW-PL-060 — shared support библиотеката вече няма „plugin" в името**: `PluginsCommonLibrary` → `DaqsterCommonLibrary` (target + `OUTPUT_NAME` + `DaqsterCommonExport.h` / `DAQSTERCOMMON_*`). Иначе `PluginDiscovery::isCandidatePluginFile()` (`toLower().contains("plugin")`) засича `libPluginsCommonLibrary.so` и логва `Bad Plugin`.
- **REQ-SW-PL-061 — runtime host плъгините преименувани + уеднаквен folder naming** (2026-10-09, клон `feat/REQ-SW-PL-056-headless-all-node-components`):
  - `FrameworkCorePlugin` → **`RuntimeHostHeadlessPlugin`**, `FrameworkGuiPlugin` → **`RuntimeHostGuiPlugin`** (класове `*Interface`/`*Object`, CMake target/`OUTPUT_NAME`, JSON `Name`, `.dll` имената в CI, коментари и docs). Имената вече съвпадат с `IRuntimeHost`/`RuntimeMode` (`Headless`/`Gui`) и не се бъркат с framework библиотеките `FrameworkCore`/`FrameworkGui`.
  - Хост-овете живеят в **семейна** директория `src/plugins/runtime_host/{headless,gui}/` (родител = роля, листо = вариант) — същият модел като `nodeditor_nodes/{core,gui}`.
  - `src/plugins/QtCoinTrader/` → `src/plugins/qt_coin_trader/` и `docs/plugins/QtCoinTrader/` → `docs/plugins/qt_coin_trader/`. Правило: plugin **директориите** са `snake_case`, C++ класове/CMake target-и са `PascalCase`.
  - JSON `PluginType` на headless хоста: `"core"` → `"headless"`; `Capabilities` вече включва `"IRuntimeHost"` при двата хоста.
- **REQ-SW-PL-062 — публичните LLM нодове в категорията `AI/LLM`** (2026-10-09, клон `feat/REQ-SW-PL-056-headless-all-node-components`): `LLamaModelDataModel` (`LLaMA Model`) и `ConsoleDataModel` (`Конзола`) се регистрират в `registerNodes()` и `registerNodesHeadless()` под `"AI/LLM"` вместо `"Sources/LLM"` — вече съвпадат с частния `LlamaGenerateModel`, така че всички LLM нодове излизат на един път в менюто. Записаните `.flow` реферират нодовете по `name()`, не по категория → без миграция.

### Fixed
- **`REQ-SW-PL-092` — анти-алайъс дефолтът пази 0.90 от изходния Nyquist, не 0.45** (2026-11-14, клон `fix/REQ-SW-PL-092-antialias-cutoff`):
  Вграденото ядро режеше на **0.45·Nyquist**: при 48 kHz → 16 kHz това е −6 dB @ **3604 Hz**, −20 dB @ 4477, −60 dB @ 5400 — тоест телефонен филтър, който изхвърля 3.6–7.2 kHz от речта (измерено спрямо `sox` референция: **−14.9 dB** в 3.6–5 kHz и **−44.0 dB** в 5–7.2 kHz). Новият дефолт е **0.90 → 7.2 kHz**, а преходът е поставен така, че изходният Nyquist (8 kHz) е ≥60 dB надолу: измерено **−6.02 dB @ 7200 Hz** и **−84.4 dB @ 8000 Hz**. Броят отводчета идва от Kaiser оценката (`tapsForTransition()`, `N ≈ (A − 8)/(2.285·Δω)` с `A = β/0.1102 + 8.7`): за ×3 тя иска **154** (64 отводчета даваха само −18 dB при 8 kHz), затова дефолтът е **160**; `kMaxTaps` става 8192, защото 160/147 (44.1 → 48 k) иска 8170. Същото правило важи за `antiAliasForResampling` (по-ниската от двете честоти). Пазачи: `TestDspFilters::antiAliasDefaultHoldsSixtyDbAtOutputNyquist` и `decimatorPassesSixPointFiveKhz` (тон 6.5 kHz след ÷3 → 0.4993, **−0.1 %**; старият филтър го убиваше).
- **`REQ-SW-PL-056` — `node_runner_headless_registry` чака изход с бюджет, не твърд таймаут** (2026-11-14, клон `fix/REQ-SW-PL-056-registry-budget`):
  `ctest -j4` падаше на `missing_node.flow` с `Process terminated due to timeout` → „AC2: expected exit code 1", а самостоятелно същият поток се отхвърляше за 0.5 s. Причината не е бавна машина: Qt Multimedia има **собствен** таймаут на свързването към PulseAudio, а докато устройството е заето (SIGTERM-нат `NodeRunner` от предходен пробег — точно състоянието, което `RESOURCE_LOCK audio` не покрива, защото ключалката пази тестове, не устройство) детето не печата **нищо** и чак след него зарежда/отхвърля потока. Измерено: същият отказ до държащ аудиото `NodeRunner` отне **30.66 s**, в лога `Using Qt multimedia` … 30.3 s по-късно … `PulseAudioService: Connection failure: "Timeout"`; твърдите 10 s отрязваха блока и обявяваха AC2 за счупен при правилно отхвърлен поток. Сега таймаутите са **бюджет с име**: `HL_PULSE_STALL_SECONDS` (35 — измерените 30.3 s, не номиналните 25) + `HL_PROCESSING_SECONDS` (10), а изчакването е **условно**, не по-дълго число: `run_headless_to_exit()` дава пълния бюджет на случаите, които излизат сами (таймаутът е таван, не изчакване → здравословният пробег пак струва 0.5 s), а `run_headless_until()` на случая, който **не** излиза сам, харчи втория прозорец само когато първият не е дал очаквания изход — тоест 10 s при здрав пробег, 10 + 45 s при блок. Провалът вече **казва причината** (`dump_and_fail()` при `timeout` изрежда PulseAudio блока, измерената стойност и `RESOURCE_LOCK audio`), а `ctest` TIMEOUT е 180 s с изписана аритметика (45 + 10 + 45 + margin) вместо 120 s по навик. **Измерено:** самостоятелно 5/5 PASS (14.6 / 40.6 / 23.8 / 40.6 / 28.6 s — четири от петте пробега са през реален блок и старият капак би ги убил), пълният `ctest -j4` 5/5 PASS 34/34.
- **`REQ-SW-PL-024` — аудио източникът може да храни няколко входа (`Out = Many`)** (2026-11-10, клон `feat/REQ-SW-PL-088-dependency-manager`):
  `AudioSourceDataModel::portConnectionPolicy()` връщаше `ConnectionPolicy::One` за **всеки** порт, тоест отменяше подразбирането на QtNodes (`In = One, Out = Many`), и `DataFlowGraphModel::connectionPossible()` отказваше втория ръб на заетия out порт — един микрофон можеше да храни **точно един** консуматор. Самият нод обаче брои връзките си (`m_connectionCount`) и заснема само при `count > 0`, а разпръскването е без копие: `onOutPortDataUpdated` подава **същия** `shared_ptr` към всеки вход. Сега `Out` → `Many`, всичко останало остава `One`. Същата политика беше прегледана при другите три източника — `PcapModel`, `JackDetectModel`, `GpuMonitorModel` също строят **нов** `SampledData` на всеки poll и никой консуматор не го променя, затова и те са `Out = Many` (с едноредов коментар защо е безопасно). Тестове: `demo_nodeditor_lifecycle_tests` +6 (`SourceFanOutTest`: втори консуматор на същия out порт минава `connectionPossible`, двамата получават **един и същ** блок по `shared_ptr`, частичното разкачване не спира другия, `In`/`None` остават `One`, трите полера също позволяват разклонение); на стария код същите тестове падат (4 от 7). As-built: `docs/plugins/nodeditor_nodes/README.md`.
- **`REQ-SW-PL-081` — директорията за ключалката се създава преди ключалката** (2026-11-08, клон `fix/REQ-SW-PL-081-ensure-data-dir`):
  `<GenericDataLocation>/Daqster` (там живеят `local-servers.lock` и `local-servers.json`) съществуваше само по стечение на обстоятелствата: `mkpath`-ът беше скрит в `acquireStartLock()`, тоест `start()` стигаше до ключалката, без никъде да е казано, че директорията трябва да я има — а на чиста машина я няма и `QLockFile` отговаря само „lock file is not writable". Сега има един `ensureDataDir()` (`mkpath` + ясна причина `не може да се създаде директорията за ключалката "<път>"`), който се вика **изрично в началото на `start()`** (преди `sweepOrphans()`/`tryLock()`, така че отказът идва синхронно и с път), в `acquireStartLock()` (покрива `launch()`, `forgetRecords()` на `stop()` и `sweepOrphans()`), в `sweepOrphans()` и в `writeRecords()`. Тестове: `TestLocalServerService` +2 — `startCreatesMissingDataDir` (`XDG_DATA_HOME` сочи към празен временен дом, `<data>/Daqster` липсва → `start()` го създава, ключалката е вътре, сървърът стига readiness) и `unwritableDataDirReportsPath` (на мястото на директорията има **файл** → `failed` с пътя в текста, `state() == Failed`, нула пуснати процеса). As-built: `docs/Architecture/local-server-service.md` → „Директорията се създава преди ключалката".
- **`REQ-SW-PL-081` — два процеса, една услуга: ключалка при старт на локален сървър** (2026-11-06, клон `fix/REQ-SW-PL-081-local-server-start-lock`):
  **Симптомът:** две стартирания на един и същи `kind` в един и същи момент (два нода в една сцена, IDE + `NodeRunner`, или две сцени) пускаха **два** сървъра — вторият на следващия свободен порт, и двата държаха тежестта си в паметта, без никой да знае за първия. **Причината:** решението „има ли вече сървър" четеше файла със собствеността, но записът се прави на `QProcess::started`, тоест между четенето и записа има прозорец: и двете стартирания виждаха празен файл и пускаха процес. **Поправката:** `LocalServerService::start()` взима **междупроцесна** `QLockFile` (`<GenericDataLocation>/Daqster/local-servers.lock`, същата директория като файла със собствеността; `tryLock(10000)` → при отказ ясна причина `не може да се вземе ключалката за услугата '<kind>' (10 s)`) и **вътре в нея** препрочита записа за `kind`: жив PID на нашия изпълним файл значи „starting или running" → `adoptExistingServer()` (преизползване, **без** втори процес), мъртъв/чужд PID се трие като остарял. Ключалката се пуска веднага щом решението е ясно — при собствен процес в `started` (или в `errorOccurred(FailedToStart)`), тоест изчакването за `/health` (минути) **никога** не е под ключалка. `launch()` я взима с 0 ms и при заета ключалка не блокира GUI, а пита `/health` отново след 200 ms (`scheduleProbeRetry()`, таван `readyTimeoutMs`) — така и две инстанции в **един** процес не се блокират взаимно. `stop()` и `sweepOrphans()` взимат същата ключалка за ≤1 s, защото и те пишат файла; при заета ключалка записът остава и следващият старт го трие като мъртъв. **Тестове:** `TestLocalServerService` +4 (`secondStartWithSameKindDoesNotLaunchSecondProcess` — две стартирания без изчакване, реалният брой процеси остава **1**; `staleRecordIsRemovedAndReplaced` — запис с мъртъв PID се заменя; `lockIsReleasedAfterStart`; `stopRemovesRecordUnderLock`), `daqster_common_tests` 14/14; `ctest` Qt6 **37/37**. As-built: `docs/Architecture/local-server-service.md` → „Два процеса, една услуга".
- **`REQ-SW-PL-050` — `ProcessOwner` не сигнализира дете, което още не е `exec`-нало** (2026-11-03, клон `fix/REQ-SW-PL-050-process-owner-starting`):
  **Симптомът:** директните пускания на `TestAsrServer` / `TestModelRefNodes` падаха в **7 от 8** случая с `QFATAL "Received signal 15"` и core dump, докато същите тестове през `ctest` бяха зелени — load-зависим флейк.
  **Причината:** `ProcessOwner::stop()` викаше `terminate()` веднага, без да погледне `QProcess::state()`. Между `fork()` и `exec()` детето е **копие на родителя** — с неговите signal handler-и, тоест SIGTERM там изпълнява handler-а на `QtTest` в детето (QFATAL + core), а не учтиво спиране. `exec()` нулира прихванатите сигнали, тоест само **след** него SIGTERM значи каквото трябва. Нодът не може да знае в коя милисекунда е `exec()` — вика `stop(0, …)` веднага след `start()`.
  **Поправката (в примитива, не в нода):** `stop()` чака `waitForStarted()` (до 500 ms) само когато `state() == Starting`; при `FailedToStart` връща **`NotRunning` без сигнал**; ако `exec()` не е станал до крайния срок — `TimedOut`, лог и **никакъв сигнал** (процесът остава вързан, за да може следващ `stop()` да опита пак). Едва след като процесът наистина е стартирал, върви досегашната последователност `terminate → wait(grace) → kill → wait(killWait)`. Инвариантът е записан в `ProcessOwner.h` и като **правило 8** в `docs/Architecture/threading.md`.
  **Тестове:** `stopWhileStartingDoesNotSignalTheChild` (наследен SIGTERM handler пише байт в маркер-файл — единственото възможно значение на байта е „сигнал преди `exec()`"; прозорецът fork→exec е разтегнат до 200 ms с `QProcess::setChildProcessModifier()`, иначе е микросекунди и тестът минава и без поправката — проверено: **3/3 FAIL** на стария код, **3/3 PASS** на новия), `stopBeforeExecReportsNotRunning` (липсващ изпълним файл → `NotRunning`, без сигнал) и `stopAfterStartStillTerminates` (регресия на учтивия път). Qt5 + Qt6 — `daqster_common_tests` 12/12.
  **Регресия на флейка:** 10 директни пуска на `TestAsrServer` и на `TestModelRefNodes`, **0/10** падания (и със, и без CPU натоварване).
- **REQ-SW-PL-078 — `Close Scene` чупеше `AudioSource` (use-after-free на capture worker-а)** (2026-11-03, клон `feat/REQ-SW-PL-078-scene-close`, Qt5 + Qt6):
  **Симптомът:** `File → Close Scene` (Ctrl+W) в NodeEditor IDE приключва с SEGV, ако сцената съдържа `AudioSource` с вързана връзка — на произволно поред затваряне, не само на първото.
  **Backtrace-ът (coredump, категоричен):** `#2 AudioSourceDataModel::setCaptureEnabled(bool)` ← `#3 AudioSourceDataModel::outputConnectionDeleted(ConnectionId)` ← `#4 QtNodes::DataFlowGraphModel::sendConnectionDeletion` ← `#5 DataFlowGraphModel::deleteConnection` ← `#6 DataFlowGraphModel::deleteNode` ← `#7 QtNodes::BasicGraphicsScene::clearScene()` ← `#8 NodeEditorIdeObject::closeCurrentScene(bool)`; скок към `0x1a0` (умрял vptr).
  **Причината:** редът на затварянето е `stop()` на всички нодове → `notifySceneClosed()` → `clearScene()`. `AudioSourceDataModel::stop()` прави `quit()` + `wait()`, а `QThread::finished → deleteLater` **унищожава worker-а** (той живее в друга нишка) — но моделът продължаваше да го държи като суров `MicCaptureWorker*`. Стъпка 3 след това трие връзките и за всяка вика `outputConnectionDeleted()` → `setCaptureEnabled(false)` → `QMetaObject::invokeMethod` върху освободения worker.
  **Поправката (в нода, не в реда на затварянето):** `m_worker` е **`QPointer<MicCaptureWorker>`** (сам става null при унищожаване) + нов еднократен флаг `m_captureStopped`, който `stop()` вдига **преди** `quit()/wait()`; нов предикат `workerReachable()` пази **всички** worker-обръщения — `start()`, `onUiStart()`, `onAudioConnectionChanged()`, `setCaptureEnabled()`. Второ `stop()` е no-op, а `stop()` не оставя pointer към worker-а. Проверени и останалите source нодове (`grep invokeMethod` в `Sources/`): `CameraSource`/`VideoFileSource`/`StreamSource` само броят връзки в `outputConnectionDeleted()`, `FilePlayback`/`NetworkSource`/`PlutoSdr` спират свои child обекти идемпотентно — единственият кръстосано-нишков `invokeMethod` е този на `AudioSource`.
  **Тестовете падат на стария код:** `demo_nodeditor_lifecycle_tests` получи три случая — `stopThenConnectionDeletedIsSafe` (граф от `DataFlowGraphModel` с `AudioSource` + консуматор, `start()`, `stop()`, после `deleteNode()` = `clearScene()`-ът; на стария код binary-то умира със SIGSEGV точно тук), `stopThenUiAndDeviceCallsAreInert` (второ `stop()`, `onUiStart`, `onAudioConnectionChanged` след спиране) и `audioSourceSurvivesFiveSceneCloses` (5 затваряния в един процес през `Daqster::stopSceneNodes()`). Свободният heap блок се залива с `0xAB` преди обръщението, защото use-after-free-ът иначе минава „на късмет" (старият vptr още е в паметта). `test_lifecycle_main.cpp` вече създава `QCoreApplication` — без него `finished → deleteLater` не се доставя и worker-ът просто не се унищожава, тоест тестът не би проверявал нищо. **Верификация:** Qt5 + Qt6 `BUILD_TESTS=ON` — 0 error / 0 warning; `ctest` 28/28 на двата. As-built: `docs/Architecture/node_editor/scene-lifecycle.md` §„След `stop()` нодът продължава да получава обръщения".
- **REQ-SW-PL-076 — Qt6 оставаше гладен: микрофонът даваше 24 % от реалното време** (2026-10-30, клон `feat/REQ-SW-PL-077-audio-sink-to-public`, Qt5 + Qt6):
  **Симптомът:** `AudioSource → AudioOutput` (микрофон → тонколони) на Qt5 е чист, а на Qt6 трака; статусът показва `ready · … · 796 underrun(s)`, тоест sink-ът е в `IdleState`, докато входът тече. Първото подозрение беше броячът — **не беше той**: измерването показа реален глад.
  **Измереното (временен `DAQSTER_AUDIO_TRACE` лог, не е в repo):** `setBufferSize(200 ms)` се приема на **двата** Qt-а (`bufferSize()` = 38 400 B при 48 kHz mono f32), `QAudioSink` на Qt6 **няма** `periodSize()`. Микрофонните блокове пристигат на **21.2 ms** средно (p50 21, max 24) по **1000 B** = 5.2 ms аудио → **24 % от реалното време**; 208 подхранвания за 4.5 s. На Qt5 същият флоу: интервал **5.3 ms**, 1000 B = 5.2 ms → **100 %**, **0** underrun. Тоест sink-ът не е дефектен — получаваше четвърт от аудиото и никакво буфериране от негова страна не можеше да го компенсира.
  **Причината (изолирана със самостоятелна проба, 2 s, PipeWire):** `QAudioSource::setBufferSize()` се чете **различно** от двата Qt-а. На Qt5 това е *период* (1000 B на всеки ~5 ms = реално време); на Qt6 това е **колко байта четем от нашето устройство на сървърен период** — при quantum 1024 кадъра (21.3 ms при 48 kHz) 1000 B на всеки 21.3 ms. Измерено: Qt5 1000 B → 192 000 B/s (100 %); Qt6 1000 B → 45 977 B/s (**24 %**); Qt6 19 200 B → 186 368 B/s (97 %). Реалният микрофон (Plantronics, 48 kHz mono S16): Qt6 1000 B → **46.9 %**, 9600 B → **99.2 %**.
  **Поправката:** нов `Sources/AudioSource/AudioCaptureBuffer.h` — `AudioCaptureBuffer::bytesFor(format)` смята буфера от **формата**, ограничен във **време** (100 ms) и с под в **кадри** (4 × 1024), защото quantum-ът е брой кадри, не време: 100 ms при 8 kHz е само 800 кадъра, тоест под един период. `MicCaptureWorker` вече иска това число вместо твърдите 1000 B.
  **Статусът (стъпка 3):** `playing` вече значи **звук**, не `ActiveState`: на Qt6 здрав push-mode sink държи ~един период и пада в `IdleState` между записите, докато току-що влязлото аудио още излиза — статусът казваше `ready` за граф, който се чува. Сега `playing` = «устройството е активно **или** има нещо в опашката», `ready` = «нищо в опашката и устройството не е активно».
  **Доказателството е записът, не броячът** (`parec` на монитора на устройството, `AudioSource → AudioOutput` 10 s, 10 ms RMS-енвелопа; Qt6): запълване **68.9 % → 100 %**, пропадания ≥ 20 ms **11 → 0**, най-дълга пауза **30 ms → 0**, RMS **0.0740 → 0.1500** (същият като Qt5 — преди беше наполовина, тоест тонът реално беше накъсан). Underrun-и: **208/4.5 s → 2/8.6 s** (остатъкът е джитер на източника). Qt5 преди/след: 100 % запълване, 0 пропадания — без регресия.
  Тестове: нов `demo_nodeditor_audiocapturebuffer_tests` (**5** случая: 48 kHz mono f32 → 4800 кадъра, «над един сървърен период» за четири формата, под в кадри при 8 kHz, мащабиране по канали/семпъл, 0 при невалиден формат) + `queuedAudioReportsPlayingEvenWhileTheSinkIdles` в `demo_nodeditor_audiooutput_tests`; `ctest` **30/30** на Qt5 и Qt6, 0 warning-а.
- **REQ-SW-PL-076 — `FilePlayback` брои интервала в кадри, не в байтове** (2026-10-30, клон `feat/REQ-SW-PL-077-audio-sink-to-public`, Qt5 + Qt6):
  **Дефектът:** `startPlayback()` смяташе `intervalMs = m_chunkSize / sampleRate * 1000`, но `m_chunkSize` е в **байтове** (това, което `QFile::read()` приема), а `sampleRate` е **кадри/сек**. При FLOAT32 (4 B/семпъл) интервалът излизаше 4× по-дълъг от аудиото в блока — 92 ms за 23 ms — тоест файлът свиреше 4× по-бавно и sink-ът оставаше гладен. При 8-битово моно (`bytes == frames`) дефектът е невидим, затова не беше забелязан.
  **Поправката:** нов `Sources/FilePlayback/FilePlaybackTiming.h` (чисти функции, тестваеми без плъгин): `chunkIntervalMs()` смята `frames = chunkBytes / bytesPerFrame()` → `intervalMs = frames * 1000 / sampleRate` (закръгляне до ms, минимум 1 ms), а `chunkBytesFor()` закръгля **байтовия** бюджет на чънка надолу до цели кадри, така че кадър никога да не се разцепва между два блока (INT24 моно: 4096 B → 4095 B). `bytesPerFrame()` идва от `SampledStreamDescriptor` — никъде не се хардкодват 2 или 4, а стерео F32 дава 8 B/кадър. При невалиден дескриптор (`bytesPerFrame == 0`, `sampleRate <= 0`, бюджет < 1 кадър) playback **не стартира**: `qCWarning(lcDemoNodes)` + статус `Playback not started: invalid stream layout …`, вместо безсмислено темпо. Последният блок се реже до цял кадър, ако размерът на файла не е кратен на кадър.
  Тестове: нов `demo_nodeditor_fileplayback_tests` (8 случая) — N кадъра при 44100 Hz → `N * 1000 / 44100` ms; F32 и S16 с еднакъв брой кадри дават **един и същ** интервал; невалидната подредба не дава интервал; бюджетът се закръгля до цели кадри; и моделът наистина стартира `QTimer` с 23 ms (F32) / 46 ms (S16) за 4096-байтов чънк. Тестът **пада на стария код** (`f32Interval` = 92 вместо 23). `ctest` **26/26** на Qt5 и Qt6, 0 error / 0 warning.
  **Реален пробег** (`FilePlayback` 44100 Hz mono F32, 440 Hz, 4.0 s → `AudioOutput`, Qt5, заснет мониторът на устройството): тонът звучи **4.01 s** с **0** пропадания ≥ 30 ms и 100 % запълване (преди поправката: 72 пропадания по ~60–70 ms и 33 % запълване, тоест към 4× по-бавно темпо); по време на свирене — **0** underrun-а, единственото съобщение е drain-ът в края на файла.
- **REQ-SW-PL-076 — ресемплерът помни състояние; форматът се договаря преди ресемплинг** (2026-10-30, клон `feat/REQ-SW-PL-077-audio-sink-to-public`, Qt5 + Qt6):
  **Дефектът (чут на реален пробег):** `AudioSource → AudioOutput` свири с вярна скорост, но звукът гъгне/трака. Причината е `AudioResample.h` — **чиста функция на блок**: `srcPos = i / ratio` от 0 за всеки блок, `outCount = llround(size * ratio)` независимо на блок и `i1 = min(i0 + 1, lastIndex)` (последният семпъл на блока се интерполира към себе си). Микрофонните блокове са ~5.7 ms → **172 граници в секунда**, всяка с фазов скок; при TTS (едно изречение = един блок, ratio 2.0) дефектът е **невидим**, защото 2.0 е цяло число и блоковата решетка съвпада.
  **(1) `AudioResample::RateConverter`** — потоков линеен ресемплер, по три числа **на канал**: `srcPos` (дробна фаза, `(-1, 0)` в началото на блок вместо `i/ratio` от 0), `lastIn` (последният входен семпъл от предишния блок — ляв съсед на интерполацията през границата) и `accum` (колко изхода вече са издадени, за да не се закръгля дължината на блок). Броят изходи се смята от **общия** брой входирани семпли (`out = floor(prev + N*ratio) - prev`), а фазовата референция стои **един входен семпъл назад**, за да не поиска последният изход на блока семпъл, който още не е пристигнал (иначе той се „заклещва" към себе си). `resampleMono()`/`resampleChannels()` остават като stateless обвивки (един блок = цял поток). Собственик е `AudioPlaybackEngine` (един конвертор за целия sink), `AudioFormatBridge::convert()` го получава по указател; състоянието се нулира при `createSink()`/`destroySink()`, при изчистване на опашката (смяна на изходния формат) и при смяна на `sampleRate`/канали в дескриптора — иначе фазата би отговаряла на изхвърлено аудио.
  **(2) Форматът се договаря преди ресемплинг** (`resolveFormat()`): след формата на източника се пробват **същата честота и канали** с друг encoding — F32 → S16 → S32 (точно тези, които `convert()` умее) — и само ако устройството откаже и честотата, се минава на `preferredFormat()` + ресемплинг. Sink на честотата на източника няма граници между блоковете, които да счупи, тоест най-доброто лечение е да не се ресемплира изобщо. Логът при отваряне казва кой вариант е приет (`device accepts the source format at 44100 Hz F32`, `device accepts 44100 Hz S16 at the source rate — re-encoded, no resampling`), а `status()` показва `converted` само когато наистина се конвертира.
  **(3) Underrun е видим.** `onSinkStateChanged()` гледаше грешката само при `StoppedState`, а push-mode sink съобщава «изчерпах се» като `IdleState` — на Qt5 с `QAudio::UnderrunError`, на Qt6 (ffmpeg backend) с `NoError`. Сега и двете се броят (`underruns()`, по **преход**, не по нотификация) и се виждат в статуса (`playing · <dev> · 48000 Hz mono · 3 underrun(s)`), логът е throttled като `logDrop()`.
  **(4) Съпътстващ билд фикс:** GUI плъгинът получава `core/shared` в include пътищата си (`AudioPlaybackEngine.h` вече включва `AudioResample.h`).
  Тестове: нов `demo_nodeditor_audioresample_tests` (QtCore-only, 5 случая — 44100→48000, 300 блока по 256 семпла: общ брой `round(total*ratio)` ±1, 300 блока == един блок от 76 800 семпла, скокът на граница ≤ скока вътре в блока + равенство с идеалната решетка, equal-rate fast path не пипа състояние, `reset()` рестартира фазата); тестът **пада на стария код** (83700 вместо 83592 семпла и граничен скок 1.0884×). `demo_nodeditor_audiooutput_tests` 30 → **32** (`encodingLadderBuildsEveryCandidate`, `conversionCarriesThePhaseAcrossBlocks`, `deviceUnderrunIsCountedAndVisible`). `ctest` **25/25** на Qt5 и Qt6, 0 error / 0 warning.
  **Реален пробег** (`FilePlayback` 44100 Hz mono F32 → `AudioOutput`, Qt6 и Qt5): `sink opened — Blackwire C5220 … · 44100 Hz mono (device accepts the source format at 44100 Hz F32)` → **converted=false**, тоест никакъв ресемплинг. `AudioSource → AudioOutput` (микрофон, 48 kHz S16): също `converted=false`; на Qt6 sink-ът съобщава подхранване на ~46 underrun/s (видими за пръв път), на Qt5 — 0 → отделен въпрос за подхранването на микрофонния поток, не реземплерът.
 таванът на блока режеше VAD сегменти; drop-oldest изхвърляше частично изсвирен блок** (2026-10-30, клон `feat/REQ-SW-PL-077-audio-sink-to-public`, Qt5 + Qt6):
  **(1) Автоарм.** `AudioPlaybackEngine::push()` при `!m_running` само трупаше в опашката, а IDE-то не вика `start()` за `IStartable` нодове (`HeadlessEngine` спазва `ui.nodes[].autoStart`, редакторът — не) → граф, отворен в редактора, беше мут и единственият изход беше бутон. Сега `AudioOutputNode::setInData()` **автоармва** преди `push()` (`enabled && !isRunning() && !userStopped()` → `start()`) — правилото, което `AudioSinkNode::play()` отдавна следва. `m_userStopped` (вдига се от `stop()`, сваля се от `start()`) пази изричната спирачка: Stop е „спри и не се вдигай сам", а не „пусни ръчно". Start/Stop бутонът остава **вторичен** — етикетът му следва `isRunning()` през новия сигнал `runningChanged(bool)` (статусът не е надежден индикатор: при грешка текстът не се меня между спряно и стартирано), така че автоармнат нод показва „Stop". Блокове след изричен `stop()` се трупат с едно `qCWarning` (`AudioOutput was stopped explicitly — N block(s) queued…`), брояч `blocksQueuedWhileStopped()` и статус `stopped · audio is queued, start to play`.
  **(2) Таван на блока 10 s → 20 s.** VAD сегментът пристига като **един** блок, а `VadNode` реже на `maxSpeechMs` = 15 000 ms — тоест 12 s реплика се отрязваше чуто. `kHardBlockCapMs` = **20 000** (над VAD тавана с 5 s резерв, ~8× измерено TTS изречение) — таванът вече се задейства само при runaway източник.
  **(3) drop-oldest не пипа частично изсвирен блок.** При `written > 0` върху главата на опашката се дропи **следващият** блок, не първият (иначе се чува скок посред изречение); ако е останал само частично изсвиреният, важи single-block изключението → нищо не се дропи. Опашката не расте: всяка итерация маха по един блок.
  Тестове: `demo_nodeditor_audiooutput_tests` **29** — нови `firstBlockAutoArmsPlayback` (sink се отваря без `start()`), `explicitStopIsNotUndoneByAutoArm`, `disabledNodeDoesNotAutoArm`, `vadSegmentLengthIsNotTrimmed` (12 s → 12 000 ms), `partiallyPlayedBlockIsNotDropped` (10 s глава + 300 ms блок → дропи се вторият, главата остава), `blockOverHardCapIsTrimmedNotSilent` (25 s → 20 000 ms); `demo_nodeditor_audiowidget_tests` **10** (`autoArmFlipsTheButtonToStop`); `ctest` **24/24** на Qt5 и Qt6, 0 warning. Реален пробег (частното, Qt6): 2-нодов граф `FilePlayback → AudioOutput` с `autoStart: false` на изхода — `HeadlessEngine` стартира само източника, а `AudioPlaybackEngine: sink opened — … Blackwire C5220 …` идва **171 ms** по-късно от първия блок, без ръчен старт, **0** дропа.
- **REQ-SW-PL-076 — опашката пази срещу натрупване, не срещу едно изречение; Qt5 изброяваше 71 ALSA устройства на всяко тикване** (2026-10-30, клон `feat/REQ-SW-PL-077-audio-sink-to-public`, Qt5 + Qt6):
  Измерено на реален пробег: английска реплика от `TtsNode` е **2.4–2.6 s** (236 800 / 249 600 B @ 24 kHz mono f32) като **един** блок, а `maxQueuedMs` по подразбиране беше 2000 ms → блокът беше **сам** в опашката, `queueMs() > maxQueuedMs` изхвърляше самия него → тишина и sink, който заради празната опашка **никога не се отваря**. Сега `maxQueuedMs` пази срещу **натрупване**, не срещу едно изречение: един блок под твърдия таван `kHardBlockCapMs` (**10 s**) **не** се дропи, колкото и да е дълъг; над тавана блокът се приема **частично** (отрязан frame-aligned до 10 s) с `qCWarning` — тишината би скрила runaway източник, а отрязаната опашка се чува като дефекта, който е; дроп остава само при реално натрупване (**повече от един** блок и `queueMs() > maxQueuedMs` → най-старото, както досега). Дефолт `maxQueuedMs` **2000 → 3000** (покрива измерените 2.4–2.6 s с резерв), граници **500–10000** (бяха 200–10000) — в `AudioPlaybackEngine`, в `AudioOutputNode::Config` и в `configSchema()`/`save()`/`load()`. Тестове: `blockOverQueueBoundDropsOldest` вече иска **два** блока (вторият избутва първия), нови `singleSentenceUnderTheOldBoundIsNotDropped` (2.6 s = 249 600 B при `maxQueuedMs: 2000` → **0** дропа) и `blockOverHardCapIsTrimmedNotSilent` (12 s блок → приети 10 s), `demo_nodeditor_audiooutput_tests` **24**.
  Втората половина е производителност: `QAudioDeviceInfo::availableDevices()` на Qt5 струва **2.4–2.8 s** на тази машина (71 ALSA устройства), а `push()`/`pump()`/`refreshStatus()` го викаха до **4× на блок**, а 20 ms помпата — при **всяко** тикване; `demo_nodeditor_audiooutput_tests` беше **250 s** на Qt5 срещу 0.7 s на Qt6. Устройството вече се **кешира** (`m_device` + `m_deviceResolvedForId` в engine-а и процесен memo по `deviceId` в `AudioPlaybackEngine.cpp`), а реално изброяване има само при смяна на `deviceId`, при (ре)старт на нода (memo на 30 s) и при грешка от sink-а (`refreshDeviceList()`); горещият път (`deviceAvailable()`, `deviceLabel()`, `provisionalFormat()`, статус, помпа) не пипа списъка. Резултат: **250 s → 2.3 s** на Qt5 (нов тест `deviceIsResolvedOnceAndCached` брои реалните резолюции през `deviceResolutions()`). Реални пробези (`video_e2e_narrate`, Qt6, английски `question`): дефолтът (без `maxQueuedMs`) → `AudioPlaybackEngine: sink opened`, **0** дропа, 1.84 s реч в monitor записа; при `maxQueuedMs: 2000` един TTS блок от **2.42 s** → пак `sink opened`, **0** дропа (старoто правило щеше да го изхвърли).
- **REQ-SW-PL-076 — `AudioOutput` изхвърляше всеки блок, по-голям от device буфера** (2026-10-30, клон `feat/REQ-SW-PL-077-audio-sink-to-public`):
  `TtsNode` издава **цялата** реплика като **един** `SampledData` (≈1.9 s = 366 KB при 48 kHz/stereo/s16; измерено 2.4–2.6 s за английско изречение при 24 kHz), а `AudioPlaybackEngine::push()` сравняваше размера на блока с `bytesFree()` на sink-а и при `free < size` го **изхвърляше** — тоест всяка реплика беше тишина при `bufferMs: 200` и дори при максимума 2000 ms. Сега блокът се приема **винаги** и се подава на устройството на парчета от опашка, ограничена по **време** (`maxQueuedMs`, дефолт 2000, граници 200–10000 — ново поле в `configSchema()`/`save()`/`load()`); `bufferMs` остава само размер на device буфера, не лимит на блока. Източването става при `push()`, при `stateChanged` на sink-а и по 20 ms таймер, който работи **само** докато има чакащо аудио (нула spin при празна опашка/липсващо устройство). Drop остава само при препълване на опашката — най-старото, с throttled лог; `stop()` и смяна на устройство/буфер изчистват опашката (PCM-ът е конвертиран за стария формат). Тестове: `demo_nodeditor_audiooutput_tests` **21** (нов: блок от 45 600 кадъра = 1.9 s @24 kHz при `bufferMs: 200` → 0 дропа; препълване на опашката → най-старото излиза; `stop()` изчиства опашката); `ctest` **24/24** на Qt5 и Qt6. Реален пробег (`video_e2e_narrate`, Qt6): `AudioPlaybackEngine: sink opened`, **0** дропа при `maxQueuedMs: 5000`, 568 000 B (5.3 s) изсвирени, чут звук в monitor записа. ⚠️ Реплика, по-дълга от `maxQueuedMs` (измерено 2.4–2.6 s > дефолтните 2000 ms), все още се изхвърля — сцената трябва да вдигне `maxQueuedMs` над дължината на изречението. **Поправено в записа по-горе по темата** (един блок вече не се дропи; дефолт 3000).
- **REQ-SW-PL-059 — `nodeWidgetDialogParent()` връщаше offscreen proxy прозореца, когато няма активен прозорец** (2026-10-29):
  Последният fallback на помощника беше `widget->window()`. За widget, вграден от `QGraphicsProxyWidget`, това е **точно** offscreen top-level-ът (`Qt::WA_DontShowOnScreen`, `parentWidget() == nullptr`), тоест при липсващ `QApplication::activeWindow()` диалогът пак ставаше transient към невидим прозорец и `exec()` увисваше — дефектът, който помощникът съществува да предотврати. Сега fallback-ът е `nullptr`: диалог без родител е пак application-modal, но е реален top-level — видим, фокусируем и затваряем. Няма промяна за вградения път, когато има активен прозорец (двата пътя са тествани от `demo_nodeditor_nodewidget_tests`, PASS на Qt5 и Qt6); `TestNodeWidgetDialogs` не проверява fallback-а и не е променян.
- **REQ-SW-PL-025 — Displays/DAQ нодовете изглеждаха без GUI: картите с графиките бяха отрязани** (2026-10-23, клон `fix/REQ-SW-PL-025-daqdisplay-sizehint`):
  `DaqDisplay`, `GenericDisplay` и `AudioDisplay` (един и същ `DaqDisplayWidget`) се зареждаха като нод **252×100**, макар да имат запазени 2 плота. Причина: `NodeGraphicsObject` оразмерява тялото на нода от `widget->height()`, а `DaqDisplayWidget` не предефинираше `sizeHint()` — `QScrollArea` отчита hint, който игнорира съдържанието си (152×21), така че целият виджет се свиваше до header реда. В резултат картите (364×211) стояха в **234×54** viewport и се виждаше само лента от тях. Сега `DaqDisplayWidget` дава `sizeHint()` = header + **две напълно видими карти** (386×460) и `minimumSizeHint()` за една карта; останалите карти се скролват. Нодът става 468×480 и двата плота се виждат. Регресионен тест `DaqDisplayNodeTest::widget_sizeHintFitsPlotCards()`; `demo_nodeditor_display_tests` **15/15** на Qt5 и Qt6.
- **Daqster launcher: child процесът не намираше библиотеките при стартиране от toolbar меню** (2026-10-24, клон `fix/REQ-SW-PL-025-daqdisplay-sizehint`):
  Когато потребителят стартира `Daqster` (toolbar) и от менюто избира `NodeEditorIDE`, child процесът наследяваше `LD_LIBRARY_PATH`, сочещ само към `../lib/` — а всички Daqster библиотеки и плъгини са в `bin/`. Резултат: `NodeditorNodesGuiPlugin` и зависимостите му не можеха да се заредят, и нодовете като `AudioDisplay` / `DaqDisplay` изглеждаха без GUI. Поправен е `ApplicationsManager::setupProcessEnvironment()` да слага `binPath` първо в `LD_LIBRARY_PATH`. Сега toolbar → NodeEditorIDE зарежда правилно и GUI-ът на Displays/DAQ нодовете се показва.
- **Daqster launcher: Qt тема не се прехвърляше към child процес** (2026-10-24, клон `fix/REQ-SW-PL-025-daqdisplay-sizehint`):
  При стартиране на `NodeEditorIDE` от toolbar меню, child процесът получаваше `QProcessEnvironment::systemEnvironment()` което НЕ включва Qt променливи зададени чрез `qputenv()` в родителския процес (напр. `QT_QPA_PLATFORMTHEME`, `QT_STYLE_OVERRIDE`). Резултатът: нодедиторът изглеждаше с дефолтна/чупена тема. Сега `setupProcessEnvironment()` прехвърля известни `QT_*` променливи от текущия процес към child-а.
- **Daqster launcher: различни билдове споделяха persistency конфиг** (2026-10-24, клон `fix/REQ-SW-PL-025-daqdisplay-sizehint`):
  `daqster` и `DaqsterAiStudio` билдовете четеха и пишеха в един `daqster_qt6.ini`. При превключване между тях, child процесът зареждаше записи от чуждия билд (чужди `.so` пътища/хешове). Сега `QPluginManager` генерира **build-специфичен** config файл: `daqster_qt6_<build>.ini` (напр. `daqster_qt6_DaqsterAiStudio.ini`, `daqster_qt6_daqster.ini`). Родител и child в един билд продължават да споделят същия persistency (бързо зареждане), а различни билдове са изолирани.
- **REQ-SW-PL-074 — LLaMA Model ↔ Конзола: старт-бутонът не се активираше, кръстосаната връзка не реагираше, отговорът се дублираше** (2026-10-17, клон `fix/REQ-SW-PL-074-llm-crosswise-flow`):
  Три регресии, внесени от split-а **REQ-SW-PL-051** (`6e47595`), който изнесе widget-а от core модела:
  **(R1)** `LLamaModelWidget::updateStartButtonEnabled()` беше добавен от split-а, но **никога не беше свързан** към `QLineEdit::textChanged` на двата пътя и не се викаше при изграждане на таба. Фабриката хидратира widget-а веднъж с `setExePath(m->exePath())`/`setModelPath(...)`; за нов нод пътищата са празни → `updateStartButtonEnabled()` заключва бутона **„Стартирай сървър"** в `disabled` **и понататъшното писане, поставяне или „..." избор през `setText()` минава през `QLineEdit`, не през `setExePath()`**, така че бутонът остава неактивен завинаги — следващото `setExePath()` от `load()` го пусна, но потребителят първо трябваше да презареди нода. Фикс: `textChanged` → `updateStartButtonEnabled()` при двата полета + извикване веднъж в `createServerTab()`; нови `objectName`-и (`exePathEdit`, `modelPathEdit`, `startServerButton`) за тестване/автоматизация.
  **(R2)** `LLamaModelDataModel::sendToModel()` мълчаливо връщаше при `!m_connected` — а защото бутона от R1 блокираше стартирането на локалния сървър, `m_connected` оставаше `false` и **всяка** заявка (от Конзола или от вградения чат) се поглъщаше без нито един признак. Сега несвързаните входове емитират диагностичен статус („Няма връзка сървъра …").
  **(R3/R4)** отговорният лямбда в `sendToModel()` емитваше `chatResponseReceived(content, exchangeObj)` **безусловно**, а `onLocalChatSend()`-овият callback го емитираше повторно → локалният чат показваше отговора **два пъти**; при заявка по порт (от Конзола) освен това се добавяше assistant съобщение в собствената чат сесия на LLaMA Model (преди split-а отговорът по порт отиваше само в JSON-дървото). Разделено на `exchangeAppended(QJsonObject)` (само JSON-дърво, при всеки отговор) и `chatResponseReceived` (само локалният чат) — свързани съответно в `NodeWidgetFactory`.
  **Протоколът не е пипан** — `{"messages","temperature","n_predict"}` от Конзола и `{"request_body","response"}` от LLaMA Model са непроменени спрямо работещата преди split-а версия. Добавени тестове: **`demo_nodeditor_llmwidget_tests`** (6 случая за enable логиката; връща **5 от 6 фала** без фикса) и **`demo_nodeditor_llmprotocol_tests`** (6 — покритие AC2–AC4 на двете посоки + tolerance към `{"message":..}`/`{"content":..}`). Пълен Qt5 + Qt6 билд, `ctest` **22/22** на двата Qt-а. *Извън обхвата:* частният `LlamaGenerateModel::setInData()` само пълни prompt полето и не стартира генерация — отделна задача.*

(fix (REQ-SW-PL-074): LLaMA Model ↔ Конзола — старт-бутон, кръстосана връзка, дублиран отговор)
- **REQ-SW-PL-059 — v2 layout: изскачащите менюта излизаха далеч от мишката и бутоните не бяха кликваеми** (2026-10-15, клон `feat/REQ-SW-PL-059-layout-designer`):
  При нодрун на `.flow` със schema-v2 `ui` секция ефект-падащото меню на видео виджетите се отваряше на случайно (отместено) място, а бутоните — напр. `Play`/`Stop` на видео сорса — не реагираха на кликове. Причина: provider-ът в `RuntimeShell::arrangeLayoutV2()` (както и дизайнерският provider и `deembedAllWidgets()`) изчистваше `Qt::Window` флага, **докато виджетът още е parentless top-level прозорец**. Графът „деембедва" нода с `NodeGraphicsObject::setWidgetEmbedded(false)`, което го показва като самостоятелен прозорец със **свой `QWindow`**; щом `Qt::Window` се махне преди reparent-а, Qt смята виджета за „вече child" и при `setParent()` **не унищожава стария `QWindow`**. `mapToGlobal()` оттогава смята спрямо него, затова popup-ите се позиционират с отместване, равно на позицията на главния прозорец, а `QApplication::widgetAt()` hit-test-ът не улучва бутоните (кликовете потъват). Фиксът маха изчистването на флаговете от provider-ите и го извършва **след** reparent-а — централизирано в `LayoutEngine::buildInto()`, който проследява подадените от provider-а виджети и чисти `Qt::Window`/`WindowStaysOnTopHint` едва когато вече са деца (покрива едновременно runtime- и дизайнерския път); `NodeEditorIdeObject::deembedAllWidgets()` вече само `hide()`-ва, без да пипа флаговете. Регресионен тест `TestLayoutEngine::deembeddedWidgetBecomesCleanChild()` (подава floated виджет и изисква `windowHandle()==nullptr`, чист `Qt::Widget` флаг и коректен `mapToGlobal()`). Ръчна проверка на `~/test.flow` под Xvfb+KWin: всички бутони/комбота дават `HITTEST … isSelf=true`, всеки дете-виджет `mapOk=true`. Qt5 + Qt6 build, `node_editor_ide_tests` **62/62**, 19/19 ctest на двата Qt-а.
- **REQ-SW-PL-059 — Layout Designer: не работеше drag&drop и не се виждаше layout-ът** (2026-10-15, клон `feat/REQ-SW-PL-059-layout-designer`):
  Два реални дефекта правеха дизайнера неизползваем. (1) Плъзгането на виджет от **Available Widgets** към **Structure** дървото никога не слагаше нода: drag source-ът пишеше id-то с `QDataStream` (binary), а drop target-ът го четеше с `QByteArray::toUInt()` (десетичен текст) → всеки payload се декодираше като нод **0**. Codec-ът вече е централизиран в `LayoutDesignerPanel::encodeNodeMime()` / `decodeNodeMime()` (и двете посоки през `QDataStream`; повреден/къс payload → `InvalidNodeId` чрез `QDataStream::status()`), плюс регресионен тест `TestLayoutDesigner::nodeMimeRoundTripsNodeId()`, който минава през реалния MIME път (старите тестове викаха само `assignSlot()`). (2) Празен layout се визуализираше като **празен preview**: `buildGrid` пропускаше празните клетки, `buildPanel` връщаше `nullptr` за празен панел, а `buildSplit` се свиваше, когато всички деца са празни — потребителят „не виждаше layout-а". Добавен е `LayoutEngine::BuildOptions::placeholders` (по подразбиране `false` = runtime непроменен); дизайнерът го вдига на `true`, така че празните панели/клетки се рисуват като пунктирани **empty** placeholders. `captureWorkspace()` продължава да чете placeholders обратно като празни слотове (не носят `layoutNodeId`), т.е. round-trip-ът е запазен. `LayoutBuildOptions` е на namespace scope (`LayoutEngine::BuildOptions` е alias), защото GCC отказва вложен клас с default member initializer като default argument на обграждащия клас. Тестове: `node_editor_ide_tests` **55/55** зелени на Qt5 и Qt6. (3) **Toolbar-ът се режеше** — един `QToolBar` не пренася на нов ред и при дефолтната ширина 1024px ~1180px бутони се препълваха, така че `Save Layout` и `Exit` тихо изчезваха в overflow менюто „»". Сега са **два реда** (ред 1: `Split/Grid/Tab/Dock`; ред 2: workspace + `+WS/−WS` + `Clear Slot/Capture/Undo/Redo` + `Save Layout/Exit`) — всичко видимо при 1024px.
- **Release tooling — `suggest_version.sh` предлагаше грешна следваща версия** (2026-10-12): baseline тагът се взимаше с `git describe`, но release таговете (`v0.3.1`/`v0.3.2`) живеят на `master` и **не са предшественици** на `develop` — затова `describe` връщаше древния `v0.0.1-beta` и класифицираше ~319 комита вместо реалните 123, предлагайки подвеждащ `1.0.0`. Сега baseline-ът идва от `VERSION` (таг `vX.Y.Z`; fallback: най-високият `v*` таг). Кодирано е и правилото от `RELEASE-PROCESS.md §1` — при `0.x` breaking промените се поглъщат от **MINOR** (MAJOR остава 0), вместо да форсират MAJOR. `BREAKING_PATHS` е стеснен до реалната host↔plugin ABI повърхност (`QPluginInterface.h`, `common/capabilities/`, `common/NodeDataTypes/`, `common/DaqsterCommonExport.h`) вместо цялата `src/plugins/common/` (18 → 13 „breaking" комита). Резултат за текущия release: `0.3.2` → **`0.4.0`**.
- **REQ-SW-PL-056 — Qt5 build на `QtNodesCore` (nodeeditor submodule)** (2026-10-11): `samiavasil/nodeeditor@305fa37` добавя липсващия `Q_DECLARE_METATYPE(QWidget*)` до `Q_DECLARE_OPAQUE_POINTER(QWidget*)`. `Q_DECLARE_OPAQUE_POINTER` сам по себе си само обръща `IsPointerToTypeDerivedFromQObject`; за да стане `QMetaTypeId2<QWidget*>::Defined = true`, Qt5 иска и `Q_DECLARE_METATYPE`. Без него `DataFlowGraphModel.cpp:297` (`QVariant::fromValue` на embedded widget) чупеше Qt5 компилацията със static_assert. Qt6 авто-регистрира указатели към QObject-производни типове, затова split-ът мина само на Qt6.
- **REQ-SW-PL-060 — застояли CI DLL имена** (2026-10-09): Windows smoke test-ът в
  `.github/workflows/build-qt6.yml` още очакваше `DemoNodeEditorNodesCorePlugin.dll` /
  `DemoNodeEditorNodesGuiPlugin.dll` след преименуването; сега проверява
  `NodeditorNodesCorePlugin.dll` / `NodeditorNodesGuiPlugin.dll`.
- **REQ-SW-PL-050 „Оставащ дълг" #1 — дублиран `IStoppable`/`IStartable`** (2026-10-09): `src/plugins/node_editor/nodeditor_nodes/core/shared/{IStoppable,IStartable}.h` стават forwarding shim-ове към каноничните `src/plugins/common/capabilities/*`. Една дефиниция → премахва риска двата байт-идентични layout-а да се разминат и cast-ът `stop()` да сочи грешно отместване. `demo_nodeditor_lifecycle_tests` вече се компилира (преди: redefinition) и `bothInterfaceHeadersDeclareTheSameType()` проверява структурната идентичност.
- **Тестова инфраструктура — side-by-side build** (2026-10-09): `DEMO_CORE_EXPORT_INCLUDE_DIR` вече се взима от `get_target_property(DemoNodeEditorNodesCore BINARY_DIR)` вместо хардкоднато `${CMAKE_BINARY_DIR}/src/...`, което чупеше `demo_nodeditor_modelname_tests` при `add_subdirectory(../daqster)` (липсващ `daqster/` префикс). Static-define-ът е `NODEDITOR_NODES_CORE_STATIC_DEFINE` (беше с невалиден `DEMO_` префикс — Windows latent).

- **REQ-SW-PL-051 — празен runtime, когато има два `IWidgetProvider`**
  (2026-10-09, клон `feat/REQ-SW-PL-056-headless-all-node-components`) —
  `RuntimeShell` и `NodeEditorIdeObject` адоптваха само **първия** provider от
  `QPluginManager::widgetProviders()` (`break; // Use the first one`). Щом две
  GUI приставки имплементират `IWidgetProvider` (напр. `DemoNodeEditorNodesGui`
  + `AiStudioPluginGui`), hash-редът на registry-то решаваше кой печели; ако
  първият не познава модела, `createWidget()` връща `nullptr`, а fallback-ът
  `embeddedWidget()` също е `nullptr` при split модел → **всички node widget-и
  изчезват и runtime canvas-ът остава празен**. Поправка: `ChatGraphModel` вече
  държи списък от provider-и и пита **всички** по ред (първият non-null печели)
  — съвпада с открай време документирания договор в `IWidgetProvider.h`
  („ask every discovered IWidgetProvider … before falling back"). Нов
  регресионен тест `test_widget_provider_fallback` (Qt5 **и** Qt6).

### Added (headless for all node components)
- **REQ-SW-PL-056 (цел A)** — `registerNodesHeadless()` вече регистрира **всичките
  33** нод типа, които има `registerNodes()`: добавени са липсващите 6 —
  `VideoOutputNode`, `DaqDisplayNode`, `GenericDisplayNode`, `AudioDisplayAlias`,
  `AudioSourceDataModelObsolete`, `AudioDisplayModelObsolete`. Паритетът е
  проверен механически (двата `registerModel<>` сет-а са идентични, в един и
  същ ред) и с новия `ctest` запис `node_runner_headless_registry`. Нито един
  от 6-те не конструира widget — съществуващото разделяне core/gui не се пипа.
  Следствие: **всички 25 `.flow` файла, които преди се отхвърляха в headless
  режим, вече се зареждат и изпълняват** (преди: 16 от 41 работещи).
  - `HeadlessEngine`: двата хардкоднати списъка „поддържани headless типове"
    са заменени с `supportedHeadlessTypes()` / `unsupportedHeadlessMessage()`,
    производни от registry-та — съобщението не може да изостане от кода.
    `errorOccurred` се логира: сигналът **нямаше нито един консуматор** в
    `src/`, така че богатата грешка никога не е стигала до потребителя.
  - `VideoOutputNode`: брояч на кадрите, пристигнали без дисплей
    (`m_ui == nullptr`), и отчет от `stop()` — „N frame(s) processed, no
    display backend attached". Това прави headless изпълнението различимо от
    „нищо не е текло", вместо да мълчи.
  - Верификация: `ctest` **18/18** Qt 6.9.2 и **18/18** Qt 5.15.2; ръчно с
    реален дисплей — `NodeRunner --headless --run tests/data/video_4views_1.flow`
    → exit 0 (грейсфулно спиране) + 4× отчета по-горе; GUI `NodeEditorIde` със
    същия flow → exit 0, 25 fps, **0** записа от новия лог (без регресия).

### Tests (core/gui split)
- **REQ-SW-PL-050** — lifecycle suite-ът вече твърди shutdown контракта на **16-те**
  достижими `IStoppable` модела, не на 4. Предишната бележка „14-те нода с
  хардуер не са тестируеми" била невярна: хардуер е нужен само на `start()`,
  а всичките 14 конструират инертно (pollers/timers/thread-ове се създават, но
  не се стартират; `QMediaDevices` само изброява и връща празно под
  `QCoreApplication`). Затова кастът от shutdown path-а, идемпотентността на
  `stop()` и трите реда на разрушаване вече се твърдят на пълния ростър, докато
  `stopWithActiveWorkIsSafe` / `startThenStopIsClean` остават на отделен тесен
  списък от 4 — „изчакване на работещ poller" не се достига без стартиране.
  Добавени са **два броещи tripwire-а** (доказано стрелят), `PlutoSdrModel` е
  включен само при намерен libiio, а очакваната стойност следва същото условие.
  Непокрити остават 2: `VideoEffectNode` (нуждава свързан `ComputePool` → тест на
  PL-039) и `VideoOutputNode` (жива GL повърхност → `demo_nodeditor_videooutput_tests`).
  Верификация: `ctest` **17/17** Qt 6.9.2 и **17/17** Qt 5.15.2.
  - **Открит, но непоправен дефект:** `StreamSourceNode` и `VideoFileSourceNode`
    правят `connect()` към `m_audioBufferOutput`, който е `nullptr` освен ако
    зареденият медиен поток няма аудио дорожка
    (`QObject::connect(QObject, Unknown): invalid nullptr parameter`). Открит е
    защото тези модели вече се конструират в suite-а; извън обхвата на този
    коммит.

### Added (core/gui split)
- **REQ-SW-PL-053** — **schema-v2 layout tree + widget builder в `node_editor_ide`**,
  пренесени дословно от `consolidate`:
  - `LayoutTree.{h,cpp}` — data моделът на v2 „ui" секцията (рекурсивно дърво от
    `split` / `grid` / `tab` контейнери с `panel` листа, плюс workspace-level
    `docks[]`) и JSON round-trip. QtCore само, без `Q_OBJECT` — тества се headless.
  - `LayoutEngine.{h,cpp}` — builder/capturer-ът, който превръща дървото в реални
    widget-и (`QMainWindow`, `QSplitter`, `QTabWidget`, `QGridLayout`,
    `QDockWidget`) и обратно. QtWidgets.
  - Парсингът е толерантен: непознат тип пада на празен panel, липсващи полета
    вземат стойности по подразбиране, а секция с `version != 2` парсва до празна
    `LayoutSection` (извиквачът остава на v1 пътя).
  - `FlowUiSection.h` се различава от consolidate само по 7-редовия документиращ
    коментар на `NodeUi::autoStart` — полето е налице и в двата, затова портът е
    дословен.
  - **Потребителите още не са закачени:** `NodeEditorIdeObject` (F11 v2 workspace) и
    `RuntimeShell::arrangeLayout` все още ползват v1 пътя. Това е следващият блок;
    дотогава тези четири TU-та се компилират и се тестват, но не са достижими по
    време на изпълнение.
- **REQ-SW-PL-053** — нов тест таргет `node_editor_ide_tests`
  (`tests/plugins/node_editor_ide/`, 21 теста: 12 за `LayoutTree` round-trip и
  tolerant parsing, 9 за `LayoutEngine` build/capture). `QApplication`, върви
  headless през offscreen platform.

- **REQ-SW-PL-055 / REQ-SW-PL-051** — **пълно развързване на Video Output node-а**
  от `_core`/`_gui`. Това е най-големият хоризонтален блок от порта на
  `consolidate_save_from_pedal`; функционалността е пренесена и едновременно
  разделена по зависимост от QtWidgets, а не по редовете на consolidate.
- **REQ-SW-PL-055** — **`IVideoOutputUi` + `IVideoOutputUiHandler`**
  (`demo_nodeditor_nodes_core/Sources/Video/IVideoOutputUi.h`): плосък non-QObject
  шев. core→core (`IVideoOutputUi`, core държи указател) и gui→core
  (`IVideoOutputUiHandler`, gui държи указател). Без moc, без link зависимост —
  споделеният тип е в `_core` заглавния файл, конкретният обект е в `_gui`.
  Редактор→модел (`setPerfEnabled`, `forceLiveDisplay`) остава през
  `QMetaObject::invokeMethod`, защото този извиквач е **трети** плъгин.
- **REQ-SW-PL-055** — **`VideoGLBlitWidget` → `demo_nodeditor_nodes_gui/Sources/Video/`**,
  дословно от consolidate. Това беше последният елемент на `EMBEDDED_WIDGETS` в
  `_core`; списъкът вече е празен и екипът `_gui` го държи за двата TU-та, които
  още чакат (`VideoEffectNode.cpp` → `QLabel`,
  `AudioDisplayModelObsolete.cpp` → `QStackedWidget`). Widget-зависимите TU-та в
  `_core` падат от 7 на 2; `Qt::Widgets` остава в link линията докато те са там.
- **REQ-SW-PL-055** — **`VideoOutputUi`** (`_gui`): целият widget дърво на node-а —
  `QStackedWidget` (preview страница / live страница), `QSplitter` (display вляво,
  контроли вдясно), perf панел с 8 етикета и effect контролите (combo + параметърни
  страници). Backend-ът се избира **веднъж** при конструкция
  (`DAQSTER_VIDEO_BACKEND=gl|software`, иначе `hasHardwareGL()`), а не на кадър.
- **REQ-SW-PL-055** — `VideoFrameData::frameToImageScaled(frame, maxWidth)`: конвертира
  **директно** в ~200 px preview вместо да материализира пълното 1080p QImage и после
  да скалира. NV12/YUV420P се субсмплират по време на YUV→RGB конверсията, така че
  пълното изображение никога не се построява (~(200/1920)² от пълна конверсия).
- **Fixed** — `VideoOutputNode::~VideoOutputNode` извикваше `stop()` →
  `resetVideoState()` → `m_ui->clearPreview()` / `display()->clear()`, докато UI-то е
  притежавано от widget дървото на редактора, а не от модела. При вграждане
  `rootWidget()` се reparent-ва в сцената, така че дървото може да унищожи UI-то
  първо и да остави модела жив → use-after-free. Указателят се занулява **преди**
  `stop()`.

- **REQ-SW-PL-055** — **`VideoDisplayWidget`** (`demo_nodeditor_nodes_core/Sources/Video/`):
  чист интерфейс за видео display повърхност — `presentFrame` / `presentImage` /
  `presentTexture` / `presentYuvTexture` / `setVideoSize` / `clear` /
  `backendName` / `isGpuBackend` / `widget()`. Умишлено **не** наследява `QWidget`:
  GL бекендът трябва да остане `QOpenGLWidget` (zero-copy презентация,
  REQ-SW-PL-053 AC1), а Qt не позволява наследяване от две QObject-производни
  класове, така че всеки бекенд комбинира интерфейса със собствения си widget
  базов клас. `widget()` връща конкретния QWidget, за да може node-ът да вгради
  display-а в layout, без да знае конкретния бекенд.
- **REQ-SW-PL-055** — **`VideoSoftwareWidget`**: software бекендът
  (`QWidget` + `VideoDisplayWidget`). GPU-resident входовете се четат обратно
  през ленивия `QImage` кеш на `VideoFrameData` (REQ-SW-PL-032) — най-много
  една конверсия на кадър. Без видео рисува placeholder-а „No video input".
- **REQ-SW-PL-051** — **`IWidgetProvider` capability**
  (`src/plugins/common/capabilities/IWidgetProvider.h`): плосък (non-QObject)
  интерфейс `createWidget(NodeDelegateModel*)` за плъгини, които доставят widget-а
  на модел, живеещ в widget-free core plugin. Открива се с `dynamic_cast` през
  `PluginRegistry::widgetProviders()`, както `INodeProvider` и `IRuntimeHost`.
- **REQ-SW-PL-051** — **`NodeWidgetFactory`** в `demo_nodeditor_nodes_gui`: 28
  creators, ключирани по `NodeDelegateModel::name()`. Widget-ите не държат pointer
  към модела — дърпат състояние чрез setter-и и връщат заявки чрез сигнали, така че
  зависимостта е еднопосочна (gui → core).
- **REQ-SW-PL-051** — **`ChatGraphModel::nodeData(NodeRole::Widget)` override** —
  единственото място в дървото, където се вика `provider->createWidget()`, с
  per-`NodeId` кеш (`QPointer<QWidget>`), чистен на `nodeDeleted` и при смяна на
  provider-а. Кешът е задължителен: QtNodes пита `NodeRole::Widget` 4–5 пъти на
  node, а нов widget без layout измерва 640×480, така че без кеш node box-ът се
  оразмерява по изхвърлен widget.
- **REQ-SW-PL-051** — **`PluginRegistry::capabilityInstances()` /
  `ensureInitialized()`** — споделени probes за node/widget providers, преизползвани
  от `instances()` и `runtimeHosts()`.
- **REQ-SW-PL-051** — `NodeEditorWidget::nodeWidget()` и `stopAllNodes()`, плюс
  `RuntimeShell` deembed през тях: plugin-и извън node editor-а не се сблъскват с
  `ChatGraphModel`.

### Changed (core/gui split)
- **REQ-SW-PL-051** — 28 от 30 node модела вече не конструират widget-и:
  `embeddedWidget()` връща `nullptr`, а widget-ът идва от GUI plugin-а. Покрити са
  video, audio, display, number, operator, file, network, system, gamepad и LLM
  моделите, плюс alias-ите им.
- **REQ-SW-PL-051** — премахнати дублираните built-in node модели в
  `node_editor_ide/BuiltInNodes/{Sources,Displays,Operators}/` и целият
  `NodeEditorBuiltInNodes` компонент; `PluginsListChangeDetected` е свързан, за да
  се откриват новозаредени external plugin-и.
- **REQ-SW-PL-051** — `PluginRegistry::nodeProviders()` и `instances(iid)` вече **не**
  инициализират plugin object, когото не са съвпаднали по capability. Иначе
  `FrameworkGuiPluginObject` — който конструира цял `RuntimeShell` (MDI +
  QtWidgets + OpenGL) в конструктора си — се издърпваше в headless процес.
- **REQ-SW-PL-051** — plugin обектите са parentless
  (`createPluginObject(hash, nullptr)`). Родителството към registry-то ги правеше
  QObject деца, а `QPluginManager::ShutdownPluginManager()` вече ги `delete`-ва
  изрично → double-free.
- **REQ-SW-PL-051** — `Daqster/main.cpp::PluginsInit()` не унищожава повече
  обектите, които създава. `deleteLater()`-ването на обект, чийто `IWidgetProvider`
  е кеширан в `ChatGraphModel`, оставяше dangling vtable и чупеше при всяко
  следващо добавяне на node.

### Fixed (core/gui split)
- **REQ-SW-PL-032** — `VideoGLContextManager::hasHardwareGL()` вече връща `false`
  вместо да се срива, когато няма `QGuiApplication`. `instance()` конструира
  `QOpenGLContext` + `QOffscreenSurface`, а нито един от двата не може да бъде
  конструиран преди съществуването на `QGuiApplication` — това е, което
  инициализира платформената интеграция, върху която са построени. Останалата
  част от функцията вече се деградира (`makeCurrent` провал → `false`), така че
  липсващият GUI стек трябва да мине по същия път. Детекция, която пада на
  въпроса „има ли GPU?", не отговаря на нищо. Засяга 5 callers-а в `develop`
  (`VideoOutputNode` ×2, `VideoEffectNode`, `CustomShaderNode` и новия
  `detectVideoBackend()`).
- **REQ-SW-PL-051** — падане при затваряне на NodeEditorIDE прозореца по време на
  възпроизвеждане на flow (SIGSEGV при exit). `NodeEditorIdeObject::
  discoverAndRegisterExternalNodes()` dereference-ваше `m_Widget` без null-проверка;
  `MainWinDestroyed()` вече беше го занулил, а plugin teardown минава след
  затварянето на прозореца. Еквивалентът в `RuntimeShell` имаше проверката.
- **REQ-SW-PL-051** — `NodeEditorWidget::stopAllNodes()` се вика при затваряне на IDE
  прозореца (`QEvent::Close` + `DeInitialize()`) — flow-ът спираше да върви.
- **REQ-SW-PL-051** — `RuntimeShell::arrangeWorkspaces()` показваше само един
  VideoOutput tab. Ползваше `model->embeddedWidget()`, който е `nullptr` за всички
  разделени модели; сега ползва `m_editorWidget->nodeWidget(nodeId)`.
- **REQ-SW-PL-051** — node-ите не се оразмеряваха спрямо widget-а си. Втори widget за
  същия модел се строеше в 6 места — всички вече ползват един кеширан accessor, а
  дублирането беше причината segfault-ът да се проявява надеждно.
- **REQ-SW-PL-051** — `NodeRunner --headless --run` падаше (exit 134/139) на flow-и с
  неподдържани headless типове. Пътят минава през `return` от `main()` преди
  `ShutdownPluginManager()`, така че lifetime грешките се проявяваха точно там — като
  heap corruption при разтоварване на `libGLX`. Сега излиза чисто с exit 1.

### Removed (core/gui split)
- **REQ-SW-PL-051** — мъртви include директории от `src/frame_work/CMakeLists.txt` и
  `src/plugins/node_editor/node_editor_ide/CMakeLists.txt`, сочещи към изтрити
  `BuiltInNodes/{Sources,Displays,Operators}/`.

### Known limitations (core/gui split)
- **REQ-SW-PL-051** — 2 от 30 node модела не са разделени. `VideoOutputNode`
  конструира `VideoGLBlitWidget` + `VideoPerfBadge` и е отложен до обединението с
  video-display веригата (REQ-SW-PL-053) — то пренарежда точно този файл и сменя
  начина на доставяне на widget-а. `AudioDisplayModelObsolete` наследява
  widget-building `QDevIoDisplayModelObsolete` от `BuiltInNodes/Library/display/`,
  затова `_core` още компилира 7 QtWidgets translation unit-а оттам.
- **REQ-SW-PL-051 AC 2 е блокиран** (същото ограничение като AC 7 на REQ-SW-PL-053):
  headless binary без `QtWidgets` не е постижим. `NodeRunner` не линква нито един
  Daqster plugin (само `FrameworkCore`), но `QApplication` (Qt5: в `QtWidgets`)
  остава необходим за headless.
- **REQ-SW-PL-051 AC 7** — unit тестове за split-а са отложени по стоящата инструкция
  „НОВИ ТЕСТОВЕ СТОП". `ctest` 11/11 на Qt5 и Qt6.

### Documentation (core/gui split)
- **REQ-SW-PL-051** — пренаписан `docs/Architecture/core-gui-split.md` в
  съответствие с реалната топология (`src/frame_work/`, `src/plugins/`,
  `src/apps/`), с `IWidgetProvider`, кеша на widget-ите, инварианта „един widget на
  node", правилата, които пазят headless режима headless, и миграционен гайд за нов
  node. REQ файлът е актуализиран с измереното състояние (28/30) и с откритите при
  прегледа регресии.
- **REQ-SW-PL-051** — plan-ът
  `DevelopmentProcess/plans/REQ-SW-PL-051-core-gui-split-implementation.md` е
  маркиран **SUPERSEDED**: описва дизайн (`DaqsterCore`/`DaqsterGui` библиотеки,
  `DaqsterHeadless`, `DAQSTER_BUILD_STATIC`), който не е имплементиран. Оставен е за
  история, не като pending work.

### Added
- **REQ-SW-PL-053** (Core/Gui plugin split — runtime, избран от plugin management):
  - **Едно dual-mode приложение `NodeRunner`**, което избира engine-а **в runtime**
    според опциите, вместо да ги линква:
    - `NodeRunner --run <flow.flow>` → `FrameworkGuiPlugin` → `RuntimeShell` (GUI)
    - `NodeRunner --headless --run <flow.flow>` → `FrameworkCorePlugin` → `HeadlessEngine`
  - **`IRuntimeHost` capability** (`src/plugins/common/capabilities/IRuntimeHost.h`):
    плосък (non-QObject) интерфейс с `runtimeMode()` / `loadFlow()` / `stopAllNodes()`.
    Открива се с `dynamic_cast`, както `INodeProvider` — не през `qt_metacast`.
  - **`QPluginManager::runtimeHosts(RuntimeMode)`** +
    `PluginRegistry::runtimeHosts(RuntimeMode)`: lazy инстанциране на plugin-ите и
    филтриране по мод. Runner-ът не реферира конкретен plugin тип — само интерфейса.
  - **`FrameworkGuiPlugin`** (`src/plugins/FrameworkGuiPlugin/`): нов plugin,
    надягащ на `RuntimeShell` (преместен от `node_editor_ide`) и `VideoGLBlitWidget`;
    притежава `RuntimeShell` като QObject дете. Само той носи QtWidgets.
  - **`FrameworkCorePlugin`** вече притежава `HeadlessEngine` и имплементира
    `IRuntimeHost` (Headless mode).
  - **Plugin isolation:** `readelf -d build_qt5/bin/NodeRunner` показва единствено
    `libFrameworkCore.so` от страната на Daqster — `NodeEditorIde`,
    `FrameworkGuiPlugin`, `FrameworkCorePlugin` и `DemoNodeEditorNodesCore` се
    резолвират изцяло чрез `dlopen`. Размер на `NodeRunner`: 74 KB → **56 KB**
    (Qt5) / 90 KB (Qt6).

### Changed
- **REQ-SW-PL-053:** `NodeEditorWidget`, `CustomDataFlowScene` и `FlowUiSection`
  преместени от plugin-а `NodeEditorIde` в споделената библиотека
  `NodeEditorLibrary`. `RuntimeShell` ги използва (registry, scene, graph model),
  така че иначе `FrameworkGuiPlugin` би трябвало да резолвира символи от друг
  plugin (`undefined symbol: NodeEditorWidget::getInjectedRegistry()`).
- **REQ-SW-PL-053:** `NodeEditorIdeObject::RunRuntime()` премахнат (мъртъв код след
  REQ-SW-PL-051); редакторът вече не изпълнява flows. F11 presentation mode
  остава редакторска функция.
- **REQ-SW-PL-053:** коректен shutdown ред в `NodeRunner` — `stopAllNodes()` се
  вика през `aboutToQuit`, **свързан преди** `QPluginManager::Initialize()`.
  `ShutdownPluginManager()` е свързан към `aboutToQuit` и унищожава plugin
  обектите (и engine-а, който притежават) *преди* `app.exec()` да върне; извикване
  след `exec()` работи през dangling pointer и дава „pure virtual method called".
  Нодовете вече се спират при SIGTERM (`HeadlessEngine: stopping all nodes`).

### Known limitations
- **REQ-SW-PL-053 AC 7 е изключен:** headless binary **без** `QtWidgets` не е
  постижим в момента. Причини: (1) в Qt5 `QApplication` е в `QtWidgets`, а
  dual-mode binary трябва да може да показва GUI; (2) node моделите конструират
  widgets в конструктора си, така че дори `QGuiApplication` build пада в
  `QWidgetPrivate::init()`. Изисква per-node core/GUI split (продължение на
  REQ-SW-PL-051).

### Documentation
- **REQ-SW-PL-054** (REST API & Remote Control) — **IMPLEMENTED (v0.3, клон
  `feat/REQ-SW-PL-054-rest-control-plane`; GUI режим от `317fe82`)**. Изискване:
  `DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-054-rest-api-remote-control.md`.
  Архитектурен документ (пренаписан 2026-10-11 като **as-built**):
  `docs/Architecture/rest-api-remote-control.md` — `ControlPlaneServer` върху
  `QTcpServer` (една реализация за Qt5/Qt6), REST + WS върху
  `NodeDelegateModel::save()/load()`, OpenAPI/Swagger UI, вграден web console.
  `RuntimeController` е имплементиран и от GUI `RuntimeShell`, така че API-ът е
  идентичен в двата runtime режима.
  Thin Client remote GUI, streaming и multi-user остават бъдещи REQ-и.
- Дублиращият се REQ-SW-PL-053 файл (`nodeeditor-plugin-split.md`) е премахнат;
  каноничен е `nodeeditoride-split.md`.

### Added (runtime mode)
- **REQ-SW-PL-048** (Runtime режим — `--run <flow.flow>`, MDI workspaces, autoStart, presentation toggle):
  - CLI: `Daqster --run <file.flow>` стартира runtime режим без editor canvas
  - `RuntimeShell` class (`src/plugins/node_editor/node_editor_ide/RuntimeShell.{h,cpp}`):
    - QMainWindow shell с QMdiArea workspaces (SDRangel модел)
    - Reuses `NodeEditorIdeObject` registration + loading logic
    - MDI layout: per-workspace QMdiArea, tabbed/SubWindow view mode
    - Two-step deembed: `setWidgetEmbedded(false)` → `QMdiSubWindow::setWidget()`
    - Window flags: `Qt::WindowStaysOnTopHint` cleared after reparenting
    - Auto-start: generic `IStartable` interface implemented by all source/sink nodes
    - Clean shutdown: `IStoppable::stop()` on all nodes on window close
  - `IStartable` interface (`src/plugins/demo_nodeditor_nodes/shared/IStartable.h`):
    - Implemented by: PlutoSdr, Pcap, VideoFileSource, StreamSource, CameraSource,
      AudioSource, Gamepad, SystemMonitor, GpuMonitor, JackDetect, FilePlayback,
      NetworkSource, FileRecord, NetworkSink, LLamaModel
    - Delegates to existing start logic (onStartRequested/onPlayPauseClicked/onConnectClicked/onStartStopClicked)
  - Presentation toggle в editor-а (F11): скрива canvas, показва деембеднати виджети
    като top-level прозорци; toggle back възстановява editor режим
  - Код: `src/apps/Daqster/main.cpp` (--run флаг), `src/plugins/node_editor/node_editor_ide/`
    (RuntimeShell, NodeEditorIdeObject), `src/plugins/demo_nodeditor_nodes/`
    (IStartable implementations)
  - Верификация: Qt5/Qt6 builds PASS + --run smoke PASS + MDI layout PASS +
    autoStart PASS + window flags PASS (Qt5/Qt6) + clean exit PASS + invalid flow
    PASS + F11 toggle PASS

- **REQ-SW-PL-049** (.flow "ui" секция — runtime layout на деембеднатите виджети):
  - `FlowUiSection.{h,cpp}` — plain structs (без Q_OBJECT): `FlowUi::Geometry`
    `{x,y,w,h,maximized}`, `FlowUi::NodeUi` (deembedded, workspace, geometry,
    autoStart), `FlowUi::WorkspaceUi` (id, geometry, tabbed), `FlowUi::UiSection`
    (version, workspaces, nodes) с `toJson()`/`fromJson()`
  - Save (`NodeEditorIdeObject::saveSceneToFile()`): граф JSON + groups
    (byte-identical на `DataFlowGraphicsScene::save()`) + `"ui"` секция;
    файлът се записва indented; geometry key се емитира само за deembedded
    нодове; custom формат `{x,y,w,h,maximized}` (НЕ `QWidget::saveGeometry`)
  - Load (`loadSceneFromFile()` + `applyUiSection()`): "ui" секцията се
    извлича преди node-cleaning loop-а и се прилага след успешен load —
    възстановява deembed state + geometry + autoStart флагове; tolerant
    guard за липсващи нодове; стар .flow без "ui" → текущо поведение;
    version > 1 → warning + празна секция
  - Код: `src/plugins/node_editor/node_editor_ide/` (FlowUiSection, NodeEditorIdeObject)
  - Верификация: Qt5/Qt6 builds PASS + ctest 11/11 green (Qt5) + FlowUiSection
    JSON round-trip smoke PASS + IDE smoke PASS (capture/apply: deembed +
    geometry + autoStart, offscreen) + load smoke PASS (ръчен .flow с "ui"
    секция, без crash) + old-flow regression PASS (без "ui", без crash);
    unit тестове отложени (стоящата инструкция „НОВИ ТЕСТОВЕ СТОП")
- **REQ-SW-PL-044** (UDP/TCP Source + Sink network DAQ nodes — MSSD framing):
  - `NetworkSourceModel` — нов **source** нод в `demo_nodeditor_nodes`,
    регистриран под `"Daq/Sources"`; 1 изходен порт `SampledData`
    (`{"sample","Sample"}`); `QUdpSocket` (UDP) / `QTcpServer` + `QTcpSocket`
    (TCP) слуша на порт; при пристигане на frame реконструира `SampledData` с
    UI-конфигурирания дескриптор (sampleRate, channels, тип INT16/FLOAT32) и
    емитира `dataUpdated(0)`; connection-count гейт (Start + поне една връзка;
    последната връзка махната → auto-stop); статус bytes received
  - `NetworkSourceWidget` — протокол (UDP/TCP), порт, sampleRate (Hz),
    channels (брой + тип), Start/Stop, статус label
  - `NetworkSinkModel` — нов **sink** нод в `demo_nodeditor_nodes`,
    регистриран под `"Daq/Sinks"`; 1 входен порт `SampledData`; при всяко
    пристигане на данни сериализира raw bytes в MSSD frame и изпраща —
    `QUdpSocket::writeDatagram` (UDP) / `QTcpSocket::write` (TCP, асинхронен
    connect); статус bytes sent
  - `NetworkSinkWidget` — протокол (UDP/TCP), адрес + порт, Start/Stop,
    статус label
  - `shared/NetworkFrame.h` — wire framing helpers: `[4-byte magic "MSSD"]
    [4-byte sampleCount][4-byte bytesPerSample][raw bytes]`, little-endian;
    UDP: 1 datagram = 1 frame; TCP: същият framing като byte stream (partial
    frames се обработват); дескрипторът се конфигурира в UI-то на двете
    страни (без out-of-band exchange за v1)
  - **Cross-platform**: Qt Network модулът е част от Qt5/Qt6 — без platform
    guard, без външни зависимости
  - `DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-044-*.md` —
    изискването (RDD gate)
  - Верификация: Qt5/Qt6 builds PASS + ctest 11/11 green (both) + headless
    smoke PASS (offscreen, plugin loads, no crash) + round-trip smoke PASS
    (реалните NetworkSinkModel/NetworkSourceModel класове, UDP + TCP на
    localhost, bytes match); unit тестове отложени (стоящата инструкция
    „НОВИ ТЕСТОВЕ СТОП“)
- **REQ-SW-PL-043** (File Record + File Playback DAQ nodes — raw bytes + JSON sidecar):
  - `FileRecordModel` — нов **sink** нод в `demo_nodeditor_nodes`, регистриран под
    `"Daq/Sinks"`; 1 входен порт `SampledData` (`{"sample","Sample"}`); при всяко
    пристигане на данни записва raw bytes във `*.sdf`; при Start записва JSON
    sidecar `*.sdf.json` (SampledStreamDescriptor); при Stop flush + close;
    статус с bytes written
  - `FileRecordWidget` — файлов път (Browse), Start/Stop запис, статус label
  - `FilePlaybackModel` — нов **source** нод в `demo_nodeditor_nodes`,
    регистриран под `"Daq/Sources"`; 1 изходен порт `SampledData`; чете
    `*.sdf` + `*.sdf.json`, реконструира `SampledStreamDescriptor`, емитира
    данните на записания sample rate (QTimer-базирано темпо, chunk 4096
    семпла); connection-count гейт (Play + поне една връзка; последната връзка
    махната → auto-stop); статус position/duration
  - `FilePlaybackWidget` — файлов път (Browse), Play/Stop, статус label
  - Файловият формат е прост и debuggable: raw interleaved sample bytes +
    човешки четим JSON sidecar (без custom binary header) — предпоставка за TX
    пътя на PlutoSDR (записан сигнал → Playback → TX)
  - **Cross-platform**: файлов I/O — без platform guard, нодовете се компилират
    навсякъде (за разлика от Linux-only System Monitor/Gamepad)
  - `DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-043-*.md` —
    изискването (RDD gate)
  - Верификация: Qt5/Qt6 builds PASS + ctest 11/11 green (both) + headless
    smoke PASS (offscreen, plugin loads, no crash) + round-trip smoke PASS
    (запис на генериран поток → Playback → валидация на bytes, дескрипторът
    се възстановява) + model round-trip PASS (реалните FileRecordModel/
    FilePlaybackModel класове); unit тестове отложени (стоящата инструкция
    „НОВИ ТЕСТОВЕ СТОП“)
- **REQ-SW-PL-042** (Gamepad input source node — Linux joystick API):
  - `GamepadModel` — нов source нод в `demo_nodeditor_nodes`, регистриран под
    `"Daq/Sources"`; 1 изходен порт `SampledData` (`{"sample","Sample"}`),
    connection-count гейт (polling-ва само докато потребителят е натиснал Start И
    има поне една връзка; последната връзка махната → auto-stop)
  - `GamepadEngine` — Linux joystick API wrapper: `open("/dev/input/js0",
    O_RDONLY|O_NONBLOCK)`, `ioctl(JSIOCGAXES/JSIOCGBUTTONS)` capability query,
    QTimer polling (30–120 Hz, default 60); осите нормализирани от
    `int16 [-32767, 32767]` до `float [-1.0, 1.0]`, бутоните `0.0` (натиснат) /
    `1.0` (отпуснат) по Linux joystick конвенцията
  - `GamepadWidget` — device path (default `/dev/input/js0`), poll rate
    (30–120 Hz, default 60), 4 axis value label-а (X/Y/Z/Rz), 8 button state
    индикатора (green = натиснат, gray = отпуснат), Start/Stop, статус label
  - Данните са `SampledData` с `domain="gamepad"` (12 канала FLOAT32:
    4 оси + 8 бутона, `sampleRate` = poll rate, `deviceId="gamepad"`,
    `sourceName="USB Gamepad"`) — консумируеми от `DaqDisplayNode` БЕЗ промени
    (waveform)
  - **Linux-only v1**: `HAVE_GAMEPAD` се дефинира в CMake чрез
    `if(NOT WIN32)` — на Windows build-ът минава без нода (XInput/DirectInput
    е бъдещо изискване)
  - `DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-042-*.md` —
    изискването (RDD gate)
  - Верификация: Qt5/Qt6 builds PASS + ctest 11/11 green (both) + headless
    smoke PASS (offscreen, plugin loads, no crash) + hardware smoke PASS
    (ShanWan USB WirelessGamepad present at /dev/input/js0, raw js_event
    stream readable); unit тестове отложени (стоящата инструкция „НОВИ
    ТЕСТОВЕ СТОП“)
- **REQ-SW-PL-041** (System Monitor source node — Linux /proc + /sys telemetry):
  - `SystemMonitorModel` — нов source нод в `demo_nodeditor_nodes`, регистриран под
    `"Daq/Sources"`; 1 изходен порт `SampledData` (`{"sample","Sample"}`),
    connection-count гейт (polling-ва само докато потребителят е натиснал Start И
    има поне една връзка; последната връзка махната → auto-stop)
  - `SystemMonitorEngine` — чете `/proc/stat` (CPU delta), `/proc/meminfo` (RAM%),
    `/sys/class/hwmon/*/temp*_input` (първият намерен сензор, милиградуси → °C) и
    `/proc/net/dev` (RX/TX bytes delta → kbps, без loopback) на QTimer (не worker
    thread — /proc четенето е <1 ms)
  - `SystemMonitorWidget` — polling interval (0.1–5.0 s, default 1.0 s), чекбокси
    за метрики (CPU/RAM/Temp/Network), Start/Stop, статус label
  - Данните са `SampledData` с `domain="system"` (5 канала FLOAT32:
    cpu_percent/ram_percent/cpu_temp_c/net_rx_kbps/net_tx_kbps,
    `deviceId="sysmon"`, `sourceName="Linux System Monitor"`) — консумируеми от
    `DaqDisplayNode` БЕЗ промени (waveform)
  - **Linux-only v1**: `HAVE_SYSTEM_MONITOR` се дефинира в CMake чрез
    `if(NOT WIN32)` — на Windows build-ът минава без нода (PDH/WMI е бъдещо
    изискване)
  - `DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-041-*.md` —
    изискването (RDD gate)
  - Верификация: Qt5/Qt6 builds PASS + ctest 11/11 green (both) + headless
    smoke PASS (offscreen) + hardware smoke PASS (Linux: /proc/stat,
    /proc/meminfo, /sys/class/hwmon, /proc/net/dev readable; RAM 68.8%,
    temp 84.0°C); unit тестове отложени (стоящата инструкция „НОВИ
    ТЕСТОВЕ СТОП“)
- **REQ-SW-PL-040** (PlutoSDR RX DAQ node — IQ streaming via libiio):
  - `PlutoSdrModel` — нов source нод в `demo_nodeditor_nodes`, регистриран под
    `"Daq/Sources"`; 1 изходен порт `SampledData` (`{"sample","Sample"}`),
    connection-count гейт (стриймва само докато потребителят е натиснал Start И
    има поне една връзка; последната връзка махната → auto-stop)
  - `PlutoSdrEngine` — libiio обвивка: `iio_create_context_from_uri(uri)` →
    `ad9361-phy` (RX LO frequency / sampling frequency / RF bandwidth / gain
    mode / hardwaregain) + `cf-ad9361-lpc` (enable `voltage0`(I)+`voltage1`(Q)) →
    `iio_buffer_refill()` в worker thread; чисто спиране с атомарен stop флаг +
    `iio_buffer_cancel()` (libiio ≥ 0.24) + thread join
  - `PlutoSdrWidget` — URI (default `ip:192.168.2.1`), честота (MHz, 70–6000),
    sample rate (MSPS, 0.2–7.5), gain mode (manual/fast_attack/slow_attack) +
    gain, Start/Stop, статус label
  - Данните са `SampledData` с `domain="iq"` (I/Q int16 interleaved,
    `deviceId="plutosdr"`, `sourceName="PlutoSky 7020-SDR"`) — консумируеми от
    `DaqDisplayNode` БЕЗ промени (waveform + FFT)
  - libiio е **опционална** зависимост (pkg-config + `find_package(libiio)`
    fallback, моделът на OpenCV): без libiio plugin-ът се build-ва без нода;
    с libiio се дефинира `HAVE_LIBIIO` и нодът се компилира + регистрира
  - `DevelopmentProcess/requirements/active/plugins/REQ-SW-PL-040-*.md` —
    изискването (RDD gate)
  - Верификация: Qt5/Qt6 builds PASS + ctest 11/11 green (both) + headless
    smoke PASS + hardware smoke PASS (PlutoSDR Rev.C fw v0.38, iio_readdev
    2.4 MSPS streamed); unit тестове отложени (стоящата инструкция „НОВИ
    ТЕСТОВЕ СТОП“)
- **REQ-SW-PL-038** (DAQSTER_AUTOSTART_FLOW — headless .flow scene load at startup):
  - `DAQSTER_AUTOSTART_FLOW=<path>` env var: NodeEditor IDE-то зарежда `.flow` сцената
    при старт без файлов диалог (за автоматизирано perf/памет тестване)
  - Зареждането минава по tolerant-пътя от REQ-SW-PL-037 — нерегистрирани нод
    типове се пропускат вместо crash; логва се `nodes=`/`connections=` бройка
  - `NodeEditorIdeObject::loadSceneTolerant(const QString& fileName = QString())` —
    празен `fileName` отваря файлов диалог (старо поведение), непразен зарежда
    директно; JSON-parse/clean/load тялото е изнесено в `loadSceneFromFile()`
  - `NodeEditorIdeObject::startVideoPlayback()` — преизползваем helper, който
    намира `VideoFileSource`/`VideoOutput` по model-name в графа, конфигурира
    source-а с `DAQSTER_VIDEO_FILE`, натиска "Play" и включва "Perf" checkbox-а;
    вика се и от `autoStartVideo()`, и от flow autostart пътя (когато
    `DAQSTER_VIDEO_FILE` е зададен)
  - `DevelopmentProcess/TEST-STRATEGY.md` — `DAQSTER_AUTOSTART_FLOW` добавен към
    env var списъка на headless тестването
  - `tests/data/video_1view.flow`, `video_2views.flow`, `video_4views.flow`,
    `video_effect_chain.flow` — тестови сцени за scene-cost измервания с
    ПУСНАТО видео (VideoFileSource → N× VideoOutput fan-out / effect chain)
  - `tools/measure_flow_memory.sh` — харнес за RSS/CPU измерване на flow сцени
    с `DAQSTER_VIDEO_FILE` (видео върви + Perf включен), PERF-верификация
    (`[PERF] video … fps=25`), 5 s RSS семпли + външно CPU семплиране от
    `/proc/<pid>/stat` (utime+stime делти — единна методология за всички
    commits, независимо от вътрешния ProcessCpu), blocking stdin
  - `tests/performance/flow-memory-perf-2026-09-02.md` — резултати: scene cost
    с видео (Qt5 + Qt6), per-view cost (~7.8 MB Qt5 / ~11.4 MB Qt6), node
    deletion memory release, Qt5 vs Qt6 сравнение
- **REQ-SW-PL-037** (NodeEditor scene save/load with missing-node handling):
  - `NodeEditorWidget::scene()` accessor — излага `DataFlowGraphicsScene*` (заедно със съществуващия `graphModel()`)
  - File меню в NodeEditor IDE-то: "Save Scene…" (Ctrl+S, `QKeySequence::Save`) и "Load Scene…" (Ctrl+O, `QKeySequence::Open`)
  - Save ползва нативния `DataFlowGraphicsScene::save()` (QtNodes файлов диалог, `.flow` JSON)
  - `NodeEditorIdeObject::loadSceneTolerant()` — толерантно зареждане: непознати нод типове се пропускат (вместо `std::logic_error` crash), връзките към тях се премахват, потребителят получава `QMessageBox::warning` с имената на пропуснатите типове
  - Тестови сцени: `tests/data/video_graph.flow` (VideoFileSource → VideoEffect → VideoOutput) и `tests/data/missing_node.flow` (познат + нерегистриран нод)
  - Допълнителни регресионни сценарии в `tests/data/`: `number_graph.flow` (NumberSource → NumberResult), `audio_graph.flow` (AudioSource → AudioDisplay), `llm_graph.flow` (LLamaModel → Console), `empty_scene.flow` (празна сцена), `malformed.flow` (скъсан JSON — трябва да не crash-ва), `multi_effect_video.flow` (два VideoEffect-а в серия)
  - `DevelopmentProcess/TEST-STRATEGY.md` — тестова стратегия: scene save/load регресионно тестване (как се ползват `.flow` сценариите, edge case-и, как се открива регресия), референция към video perf тестването и verification gate-а от RDD-PROCESS.md
- **REQ-SW-PL-039** (common compute pool — споделен ComputePool + миграция на CPU пътищата):
  - `src/plugins/common/Threading/ComputePool.{h,cpp}` — процесно-споделен `QThreadPool` singleton (вместо per-node pool-ове); per-key "latest-wins" submission (frame skipping) + per-key serialization, така че worker-only ring buffer contract-ът на DaqDisplayNode се запазва; `submitLatest(key, task)` / `cancel(key, timeout)` / per-key метрики (`submitted`/`started`/`completed`/`skipped`/`fps`); `DAQSTER_COMPUTE_THREADS` env var за брой worker нишки
  - `DaqDisplayNode` мигриран от node-owned `QThreadPool(maxThreadCount=1)` към споделения pool с per-node key — `onRefreshTick()` подава lambda към `ComputePool::instance().submitLatest()`, деструкторът вика `cancel(key)` вместо `clear()+waitForDone()`; `m_computeInFlight` махнат (pool-ът следи busy-ness per key)
  - `VideoEffectNode` CPU path мигриран към pool-а: GPU path-ът остава на GUI thread (GL-bound); CPU path snapshot-ва input-а на GUI thread (implicit-share `QVideoFrame` copy за CPU-resident, `asImage()` readback за GpuRgba) и подава `applyCpu()` към pool-а — worker-ът конвертира собственото си копие и връща резултата през queued `onCpuResult()` slot
  - `VideoEffectNode` метрики: `m_totalFrames` брояч, widget label `CPU <completed>/<submitted> · <skipped> skipped · <fps> fps out` (refresh на всеки CPU резултат) и optional `[PERF] effect` console line (5 s timer, само когато "video" perf domain е enabled)
  - Тестове: `demo_nodeditor_videoeffect_tests` (CPU path async резултат, GpuOrCpu fallback, metric label, totalFrames); `ComputePool.cpp` добавен към display test target
- **Node palette (nested categories + Simulink-style scheme)**:
  - `nodeeditor` submodule: `DataFlowGraphicsScene::createSceneMenu()` строи nested категорийно дърво — category string-ове се split-ват на `/` (напр. `AI/LLM` → AI → LLM); node имената никога не се split-ват (защита за имена, съдържащи `/`, като `Arithmetic/Logic`); филтърът работи с произволна дълбочина (parent walk-up)
  - Нодовете са прекатегоризирани по Simulink-style схема: Daq (DaqDisplay, GenericDisplay → `Daq/Display`), Audio (AudioSource → `Audio/Sources`), Video (Camera/VideoFile/Stream → `Video/Sources`; VideoEffect/CustomShader/FrameSampler → `Video/Processing`; VideoOutput → `Video/Display`), AI (FrameToTensor → `AI/Preprocessing`, LLamaModel → `AI/LLM`), General (NumberSource → `General/Sources`; Modulo/ArithmeticLogic → `General/Processing`; NumberDisplay/Console → `General/Display`), Obsolete (всички obsolete нодове → `Obsolete`)
- **REQ-SW-PL-045** (GPU Monitor source node — NVIDIA NVML):
  - `GpuMonitorEngine` — NVML обвивка: `nvmlInit()`, handle за GPU 0, QTimer polling (default 1s), `nvmlShutdown()` при stop; чете utilization/memory/temperature/power/fan/clock
  - `GpuMonitorModel` (`NodeDelegateModel`) — 1 изходен порт `SampledData` ("sample"), connection-count gating (auto start/stop), обвива метриките в `SampledData` с `SampledStreamDescriptor` (domain="gpu", 6 FLOAT32 канала: gpu_util/mem_used/gpu_temp_c/power_w/fan_pct/clock_mhz, sampleRate=1/interval), емитира `dataUpdated(0)`; save/load на interval
  - `GpuMonitorWidget` — UI: polling interval (0.1–5s), Start/Stop, статус (GPU name + metrics)
  - Регистрация като `"Daq/Sources"` в `registerNodes()` (охранена с `#ifdef HAVE_NVML`)
  - NVML — опционална зависимост (`find_library` + `find_path`, модел на OpenCV): без NVML build-ът минава без нода
  - Код: `src/plugins/demo_nodeditor_nodes/Sources/GpuMonitor/`
- **REQ-SW-PL-046** (Jack-detect source node — HDA jack events):
  - `JackDetectEngine` — сканира `/proc/asound/card*/codec#*/jack*`, парсва jack имената и състоянията (`Pin 0x21 (Headphone): present = No`), QTimer polling (default 500ms), детектира промени (event-driven `jacksChanged()`)
  - `JackDetectModel` (`NodeDelegateModel`) — 1 изходен порт `SampledData` ("sample"), connection-count gating (auto start/stop), обвива jack-овете в `SampledData` с `SampledStreamDescriptor` (domain="jack", deviceId="hda", sourceName="HDA Jack Detect", динамични FLOAT32 канали — по един на jack, стойности 0.0/1.0), емитира `dataUpdated(0)`; save/load на interval
  - `JackDetectWidget` — UI: polling interval (0.1–5s, default 0.5), Start/Stop, статус (списък на jack-овете и състоянията им)
  - Регистрация като `"Daq/Sources"` в `registerNodes()` (охранена с `#ifdef HAVE_JACK_DETECT`)
  - Linux-only платформен guard (`if(NOT WIN32)` → `HAVE_JACK_DETECT`): на Windows build-ът минава без нода; без jack файлове нодът репортва празен статус без crash
  - Код: `src/plugins/demo_nodeditor_nodes/Sources/JackDetect/`
- **REQ-SW-PL-047** (pcap Packet Capture source node — libpcap):
  - `PcapEngine` — libpcap обвивка: `pcap_open_live()`, `pcap_compile()`/`pcap_setfilter()` за BPF, `pcap_loop()` в worker thread (QThread), `pcap_breakloop()` за спиране, `pcap_close()` при деструктор; thread-safe packet queue към Model
  - `PcapModel` (`NodeDelegateModel`) — 1 изходен порт `SampledData` ("packet"), connection-count gating (auto start/stop) + user Start/Stop, обвива пакетите в `SampledData` с `SampledStreamDescriptor` (domain="pcap", deviceId=interface name, sourceName="pcap capture", BYTES канал за payload, sampleRate=0 event-driven), metadata (timestamp, caplen, len) в пакета; емитира `dataUpdated(0)`; save/load на interface/filter/snaplen/promiscuous
  - `PcapWidget` — UI: interface selector (dropdown от `pcap_findalldevs()`), BPF filter text field (напр. "tcp port 80"), snaplen spin box, promiscuous checkbox, Start/Stop, статус (packets captured, kernel drops, interface drops)
  - Регистрация като `"Daq/Sources"` в `registerNodes()` (охранена с `#ifdef HAVE_PCAP`)
  - libpcap — опционална зависимост (`find_library` + `find_path`, модел на NVML/OpenCV): без libpcap build-ът минава без нода; Windows guard за WinPcap/Npcap (`wpcap`/`Packet` библиотеки)
  - Код: `src/plugins/demo_nodeditor_nodes/Sources/Pcap/`

### Fixed
- **REQ-SW-PL-039** (CPU effects work in the display — VideoOutputNode embedded effects):
  - `VideoOutputNode` embedded effect block: CPU-only ефекти (blur/gaussianBlur/canny/threshold) вече работят и върху GpuRgba вход — `gpuApplied` флаг следи дали GPU path-ът е произвел output-а и CPU path-ът (с `asImage()` readback, като `VideoEffectNode`) се изпълнява винаги, когато GPU path-ът не е приложен (преди `if (!videoFrame->isGpuRgba())` пропускаше CPU ефекта за GPU-resident вход)
  - `VideoFrameData::asImage()` Qt5: NV12/YUV420P → QImage ръчна BT.601 конверсия (`yuvToImage()`) — Qt5 `QVideoFrame::image()` връща null за YUV формати, така че CPU ефектите и software display path-ът не работеха върху реални (NV12 owned-copy) кадри
  - `VideoOutputNode` effect combo: смяната на ефекта вече вика `reprocessCurrentFrame()` (като `VideoEffectNode`) — при paused source ефектът се прилага веднага, не чак на следващия кадър
  - Тестове: `demo_nodeditor_videoframe_tests` (Qt5 NV12 + YUV420P → QImage), `demo_nodeditor_videooutput_tests` (CPU ефект върху GpuRgba вход с реален GL texture readback)
- **REQ-SW-PL-039** (Qt6 worker frame conversion — thread-safe CPU converter):
  - `VideoFrameData::frameToImageCpu()` — нов public static converter: pure-CPU QVideoFrame→QImage от ВСЯКА нишка (без GL/RHI). NV12/YUV420P през BT.601 `yuvToImage()` (вече public static и version-agnostic — Qt5 и Qt6); RGB формати (RGB32/ARGB32/RGB888 и др.) чрез директно wrapping на mapped bits + deep copy (без pixel conversion). Връща null само за неподдържани формати или failed map
  - `VideoEffectNode` ComputePool worker: `frameCopy.toImage()` (Qt6 — RHI/GPU conversion на worker нишката, забранено от Qt, QTBUG-131107) / `frameCopy.image()` (Qt5 — null за NV12) заменени с `VideoFrameData::frameToImageCpu()` и на двата Qt; GpuRgba pre-readback fast path-ът е непроменен
  - Тестове: `demo_nodeditor_videoframe_tests` — `frameToImageCpu_*` (RGB32/ARGB32 wrap, NV12/YUV420P BT.601, unsupported → null) и на Qt5, и на Qt6

### Refactored
- **REQ-SW-PL-050** (node thread lifecycle — IStoppable interface):
  - **Дизайн решение (потребител, 2026-09-08): НЯМА промени по външни
    submodules (nodeeditor).** Първоначалната имплементация добавяше
    `virtual void stop()` в `NodeDelegateModel.hpp` + `deleteNode()` викаше
    `model->stop()` в `DataFlowGraphModel.cpp` — **revert-ната** (submodule
    върнат към `906e300`).
  - Вместо това Daqster дефинира `shared/IStoppable.h` в
    `demo_nodeditor_nodes`: чист интерфейс `Daqster::IStoppable` с
    `virtual void stop() = 0` (идемпотентен)
  - 18 node модела с background работа (threads/timers/processes) вече
    имплементират `IStoppable` вместо override на `NodeDelegateModel::stop`:
    PlutoSdr, Pcap, AudioSource, VideoEffect, LLama, Gamepad, SystemMonitor,
    GpuMonitor, JackDetect, VideoFileSource, StreamSource, CameraSource,
    VideoOutput, DaqDisplay, FileRecord, NetworkSink, FilePlayback,
    NetworkSource — деструкторите викат `stop()` като единствен shutdown path
  - Framework fix-ът остава: `QPluginManager::ShutdownPluginManager()`
    синхронно изтрива plugin instances след `shutdownAll()` (join на нишките
    докато event loop-ът е жив) + `PluginRegistry::allPluginInstances()`
  - Верификация: Qt5/Qt6 builds PASS + headless crash test 3× EXIT 0
    (SIGTERM с активни нишки) + `nm -D` stop symbols 62
- **Display consolidation (SampledData display world → DaqDisplayNode)**:
  - `AudioDisplayAlias` (DemoNodeEditorNodesObject.cpp) — нов тънък alias на `DaqDisplayNode` (само `name()` → `"AudioDisplay"`), заменя `AudioDisplayModelObsoleteAlias`; регистрацията на ключа `"AudioDisplay"` е преместена от `Obsolete` в `Daq/Display` — старите saved графи вече разрешават към реалния multi-plot/FFT/ring-buffer SampledData display, а не към QDevIO obsolete нода
  - `AudioDisplayModelObsolete` (name `"AudioDisplayObsolete"`, категория `Obsolete`) остава непроменен — QDevIO display world-ът живее за старите QDevIO графи
  - `GenericDisplayNode.h` — премахнат redundant `dataArrivalChangesWidget()` override (базата `DaqDisplayNode` вече връща `false`)
  - Тестове: `demo_nodeditor_display_tests` — `genericDisplay_aliasBehavior()` (name/save/restore parity) + `registry_aliasResolution()` (registry `create("GenericDisplay")`/`create("AudioDisplay")` връщат DaqDisplayNode-производни модели); `GenericDisplayNode.cpp` добавен към display test target
  - Нов `.flow` fixture: `tests/data/display_aliases.flow` — `AudioSource` → `DaqDisplay` + `GenericDisplay` + `AudioDisplay` (4 нода, 3 връзки)

## [0.3.2] - 2026-09-02

### Fixed
- **REQ-SW-PL-036** (deploy all Qt modules + OpenSSL in Windows release packaging)
- **REQ-SW-PL-036** (explicitly copy Qt WebSockets + required Qt DLLs in Windows packaging)
## [0.3.1] - 2026-09-01

### Fixed
- **REQ-SW-PL-036** (Deploy & Artifact-Packaging Fixes):
  - `tools/create_appimage.sh`: премахнат `QT_DIR=""` clobber-ът — `QT_DIR` вече идва от env var / `--qt-dir` (CI може да подаде правилната Qt инсталация)
  - `tools/create_appimage.sh`: бандлване на OpenSSL (`libssl.so.*` + `libcrypto.so.*`) — нужни на QtCoinTrader plugin-а при runtime в AppImage
  - `tools/create_appimage.sh`: post-build ldd верификация на `usr/bin/Daqster` и всеки plugin `.so` — fail-ва при неизвестни shared library зависимости
  - `.github/workflows/ci.yml`: `qtwebsockets` добавен към aqtinstall modules (Qt6 Linux + Windows) и `libqt5websockets5-dev` към apt (Qt5 Linux)
  - `.github/workflows/ci.yml`: Qt version smoke check — извлича AppImage-а и верифицира че бандлнатият Qt е 6.8.x
  - `.github/workflows/release.yml` Linux: `ubuntu-22.04` → `ubuntu-24.04`, system Qt → aqtinstall Qt 6.8.3, добавени GStreamer runtime + Mesa GL пакети, `-DCMAKE_PREFIX_PATH="$QT_ROOT_DIR"`, `QT_DIR="$QT_ROOT_DIR"` към create_appimage.sh
  - `.github/workflows/release.yml` Windows: премахнати `qtdeclarative qtsvg` от modules (BASE пакети за 6.8.3), добавено `qtwebsockets`; windeployqt ползва `QT_ROOT_DIR` вместо `Qt6_Dir`
  - `CMakeLists.txt`: добавено `print_component_status_summary()` след `print_build_configuration_summary()`
  - `tools/create_appimage.sh`: бандлване на FFmpeg библиотеките (`libav*`/`libsw*`) — Qt6 Multimedia FFmpeg backend-ът (`libffmpegmediaplugin.so`) линква срещу тях; без тях plugin-ът не може да се зареди (dlopen) и видео/аудио не е налично
  - `tools/create_appimage.sh`: предупреждение когато Qt multimedia plugin директорията не е пакетирана
  - `.github/workflows/ci.yml`: smoke test-ът върви с `QT_DEBUG_PLUGINS=1` и fail-ва при "No QtMultimedia backends" / "Cannot load library ... ffmpegmediaplugin"

### Changed
- **REQ-SW-PL-036** (CI/CD — shared Qt6 build pipeline):
  - Нов `.github/workflows/build-qt6.yml` — reusable workflow (`workflow_call`) със `linux-qt6` + `windows-qt6` jobs, извлечени от доказаната версия в `ci.yml` (Windows: Ninja + `ilammy/msvc-dev-cmd` + `CMAKE_PREFIX_PATH`; Linux smoke test с `QT_DEBUG_PLUGINS=1` + FFmpeg backend check)
  - Inputs: `upload-artifact-name` (required), `create-checksums`, `verify-qt-version`, `export-compile-commands` (boolean, default false)
  - `.github/workflows/ci.yml`: `linux-qt6` и `windows-qt6` вече викат reusable workflow-а (една source of truth, без drift); `linux-qt5` compat job-ът остава както е
  - `.github/workflows/release.yml`: `linux-qt6` и `windows-qt6` викат reusable workflow-а с `create-checksums: true`; tag-verification step-ът е изнесен в отделен `verify-tag` job (fail-fast преди build-овете); `create-release` job-ът е непроменен (artifact names съвпадат)

## [0.3.0] - 2026-08-30

### Added
- **REQ-SW-PL-035** (Version Centralization — single source of truth & semver bump check):
  - Нов `VERSION` файл в корена на repo-то — единствен източник на истина; root `CMakeLists.txt` чете версията от него (без хардкод)
  - Генериран `daqster_version.h` (`DAQSTER_VERSION_MAJOR/MINOR/PATCH` + `DAQSTER_VERSION_STRING`) при configure
  - `create_plugin()` разпространява версията до всички плъгини (вкл. legacy QtCoinTrader + тестовите) чрез include dir + `DAQSTER_PLUGIN_VERSION` compile definition
  - Всички плъгини на **0.3.0**; `main.cpp` ползва `DAQSTER_VERSION_STRING`
  - `scripts/version.sh` — subcommand интерфейс `get`/`set`/`check`; `version-sync.yml` ползва `version.sh check`
  - `scripts/suggest_version.sh` — release-time semver bump check (feat → minor, fix → patch, breaking → major)
  - Плъгин JSON metadata файловете (8) се **генерират** от `.json.in` шаблони чрез `configure_file()` с `@DAQSTER_VERSION@` (вместо хардкод); `version.sh set` вече не ги редактира, `version.sh check` проверява шаблоните + генерираните JSON-и
- **REQ-SW-PL-028** (VideoEffectNode — GPU/CPU backend по ефект):
  - `VideoGLShaders.h` — споделени GLSL source builder-и (`buildVertexSource`, `buildYuvFragmentSource`, `buildRgbaFragmentSource` извлечени от `VideoGLBlitWidget.cpp` + нов `buildEffectFragmentSource` с `u_flipY` и инжектируем effect body)
  - `VideoEffectOps.{h,cpp}` — `EffectSpec` регистър (7 ефекта: brightness, contrast, grayscale, invert, sepia, channelSwap, flip) с CPU функции (делегират на `VideoTransformOps`) + GLSL body
  - `VideoEffectGLProcessor.{h,cpp}` — GPU backend: `QOpenGLContext` + `QOffscreenSurface` + `QOpenGLFramebufferObject`, Y/U/V upload с `GL_UNPACK_ROW_LENGTH`, `hasHardwareGL()` (llvmpipe/softpipe/SwiftShader детекция, lazy кеширана)
  - `VideoEffectNode.{h,cpp}` — нод модел (port 0 in/out `VideoFrameData`) + **един нод с комбобокс** за избор на ефект (комбо + `QStackedWidget`, като `VideoTransformNode`), runtime backend избор, параметър UI (slider/combo/info), save/load с clamp-ове (backward compatible: `"effect"` = id + параметри)
  - **Blur + OpenCV ефекти (2026-08-26, комит `095981b`)** — добавени `blur` (box blur, radius 0..10) + `gaussianBlur`/`canny`/`threshold` (OpenCV, само при `HAVE_OPENCV`) като `CpuOnly` ефекти — общо 11 ефекта; покриват всички операции на премахнатия `VideoTransformNode`
  - Регистрирани под категория "Video" в demo node editor plugin
  - Комбобоксът на VideoEffectNode показва backend-а на всеки ефект като суфикс ((GPU)/(CPU)); registry ключът остава spec.id
- **REQ-SW-PL-030** (FrameSampler — ресемплиране):
  - `FrameSamplerNode.{h,cpp}` — отделен нод (port 0 in/out `VideoFrameData`), режими „Every N-th frame" (1..1000) / „Max FPS" (1..120), zero-copy passthrough (същият `shared_ptr`), gate без emit при не-pass, save/load + reset на брояча/таймера
  - Регистриран под категория "Video" в demo node editor plugin
- **REQ-SW-PL-032** (VideoFrameData lazy QImage кеш):
  - `VideoFrameData::asImage()` — lazy CPU QImage кеш: конверсията става най-много веднъж на кадър, резултатът се кешира и се споделя между всички CPU консуматори (fan-out); `setFrame()` инвалидира кеша; GUI-thread only (без mutex)
  - Консуматорите ползват кеша: `VideoEffectNode` CPU път + `VideoOutputNode` software display / downstream consumer път (`VideoCompat::frameToImage` → `asImage()`); `VideoGLBlitWidget` остава на `frameToImage` (raw `QVideoFrame`, не `VideoFrameData`)
- **REQ-SW-PL-032** (GPU-resident транспорт, Stage 2A — споделен GL контекст + текстура транспорт):
  - `VideoGLContextManager` (`src/plugins/common/GL/`) — process-wide споделен `QOpenGLContext` + `QOffscreenSurface` (share group с `QOpenGLWidget` display-а), lazy `hasHardwareGL()` (llvmpipe/softpipe/SwiftShader детекция), `deleteTexture()`; GUI-thread only
  - `VideoFrameData::asTexture()` — lazy GPU текстура кеш: Y/U/V upload веднъж на кадър (NV12/YUV420P), handle-ът се кешира и се споделя между всички GPU консуматори; `fromTexture()` — wrap-ва GPU-resident RGBA текстура (ефект изход); `isGpuResident()`/`isGpuRgba()`; текстурите се изтриват в деструктора
  - `VideoEffectGLProcessor` ползва споделения контекст (вместо собствен)
- **REQ-SW-PL-032** (GPU-resident ефект верига, Stage 2B — текстура-вход/текстура-изход, без readback):
  - `VideoEffectGLProcessor::processTexture()` — bind-ва входните текстури от `VideoTextureHandle` (без upload), избира програма по layout (nv12/420p/rgba), рендери в FBO и връща RGBA текстурата (без `toImage()`); нова `buildRgbaEffectFragmentSource()` (RGBA sampler + effect body + u_flipY/u_brightness/u_contrast); изходната текстура се създава на повикване и ownership-ът се предава на `fromTexture()`
  - `VideoEffectNode` GPU път: `asTexture()` → `processTexture()` → `fromTexture()` — вторият ефект в синджир консумира текстурата директно (без upload/readback); CPU fallback-ът остава
  - `VideoGLBlitWidget::presentTexture()` — zero-copy display на GPU-resident RGBA текстура (bind, без upload/readback); `VideoOutputNode` GL blit пътят я ползва за GpuRgba кадри; Qt6 native display прави readback на границата (`presentableFrame` → `asImage()`)
  - Smoke driver: `DAQSTER_AUTOSTART_EFFECT2=<effectId>` вмъква втори VideoEffect нод (GPU-resident верига)
- **Smoke drivers** (`NodeEditorIdeObject.cpp`): `DAQSTER_AUTOSTART_EFFECT=<effectId>` вмъква VideoEffect нод между source и output; `DAQSTER_AUTOSTART_EFFECT2=<effectId>` вмъква втори; `DAQSTER_AUTOSTART_SAMPLER=1` вмъква FrameSampler
- **REQ-SW-PL-029** (CustomShaderNode — общ GPU compute нод с runtime GLSL):
  - `CustomShaderNode.{h,cpp}` — node model (port 0 in/out `VideoFrameData`), GLSL редактор + compile button + error log + uniform controls, mainImage contract (UI се строи в `buildWidget()`, `CustomShaderNode.cpp:117-188`)
  - `CustomShaderGLProcessor.{h,cpp}` — GPU processor: runtime GLSL compile (`addShaderFromSourceCode`), per-(effect,layout,profile) program cache, YUV→RGBA pre-pass, error handling без crash
  - `texture()` compat fix — GL profile detection и използване на `texture()` или `texture2D()` според GLSL version
  - Комити: `42ba334` (feat: CustomShaderNode), `0682c1b` (fix: texture() compat)
- **Scene invalidation fix (REQ-SW-PL-032):**
  - `onNodeDataArrived` + `dataArrivalChangesGeometry` — nodeeditor scene invalidation fix, предотвратява ненужни repaint-и при пристигане на данни
  - `CustomDataFlowScene` override + 3 video nodes opt-out — repaint-only optimization за video нодовете (VideoEffectNode, FrameSamplerNode, VideoOutputNode)
  - Комити: `d4a90ee` (fix: onNodeDataArrived + dataArrivalChangesGeometry), `8418f53` (feat: CustomDataFlowScene override + 3 video nodes opt-out)
- **Video нодове** (`src/plugins/demo_nodeditor_nodes/Sources/Video/`):
  - `VideoCompat.h` — Qt5/Qt6 multimedia абстракция (QVideoProbe ↔ QVideoSink, camera enumeration, media source assignment, playback-state сигнали)
  - `CameraSourceNode` — заснемане от QCamera (default или избран device)
  - `VideoFileSourceNode` — възпроизвеждане на локален видео файл (QMediaPlayer)
  - `StreamSourceNode` — възпроизвеждане на HTTP/RTSP stream (QMediaPlayer)
  - `VideoOutputNode` — live preview на incoming кадри (QLabel, pass-through)
  - `VideoModifierNode` → `VideoTransformNode` (REQ-SW-PL-019) — конфигурируем transform node: 8 базови операции (RGB Channel Swap, Grayscale, Invert, Brightness, Contrast, Blur, Flip, Sepia) + опционални OpenCV операции (GaussianBlur, Canny, Threshold) при открит OpenCV
  - Регистрирани под категория "Video" в demo node editor plugin
- **Requirements Manager plugin** (`src/plugins/requirements_manager/`):
  - Standalone application plugin с requirements дърво (REQ-SW-002..008)
  - Навигация: auto-clear filter при линк клик + back/forward история
  - Search engine (REQ-SW-PL-011)
  - Multi-repository requirements view — merge на public/private requirements (REQ-SW-PL-012)
- **Dependency graph viewer** (REQ-SW-009) — интерактивен граф на зависимостите между requirements
- **Traceability matrix** (REQ-SW-010) — изглед и export на traceability матрицата
- **Typed requirement IDs** — поддръжка на `REQ-SW-<TYPE>-NN` схема с формат валидация (с миграция на съществуващите ID-та)
- **Sugiyama auto-layout** (REQ-SW-PL-016) — автоматично подреждане на dependency graph
- **Phase status indicators** (REQ-SW-PL-017) — индикатори за фаза/статус в requirements main view
- **Plugin security & vendor trust store** (REQ-SW-FW-007) — ново изискване за plugin сигурност
- **Unit тестове за REQ-SW-PL-011/012/017/018/019** (2026-08-07, Qt5 5.15.2 + Qt6 6.9.2 PASS):
  - requirements manager shared binary `requirements_manager_tests` **87/87** (TestSearchEngine 19, TestMerge 6, validator +4, TestParser +5 phaseStatus) + `requirements_manager_matrix_tests` 7/7 + `requirements_manager_exporter_tests` 7/7
  - video `demo_nodeditor_nodes_tests` **25/25** (16 VideoTransformOps + 9 StreamUrlValidator)
  - комити: `825a9b4` (PL-017), `dff234a` (PL-011), `0429ea8` (PL-012), `3048fbd` (refactor: StreamUrlValidator), `0cf6d19` (fix: Qt5 vertical flip), `bac4503` (video suite), `9d90fff` (fix: exporter CSV header)
- **ArithmeticLogic нод** (`BuiltInNodes/Operators/ArithmeticLogic/`):
  - `ExprParser` — recursive descent C++ expression evaluator (всички C/C++ оператори: `+`,`-`,`*`,`/`,`%`,`&`,`|`,`^`,`~`,`<<`,`>>`,`&&`,`||`,`!`,`==`,`!=`,`<`,`>`,`<=`,`>=`,`?:`)
  - `ArithmeticLogicModel` — конфигурируем нод: тип (int/double), 2–8 входа, expression field, optional strobe
  - Променливи `a`–`h` отговарят на входните портове
- **Framework Architecture Refactoring** - голям рефакторинг за извличане на reusable компоненти:
  - **Platform Abstraction Layer** (`frame_work/base/src/platform/`):
    - `ShutdownHandler` - абстрактен базов клас за graceful shutdown
    - `UnixShutdownHandler` - SIGINT/SIGTERM signal handling за Unix/Linux (self-pipe)
    - `WindowsShutdownHandler` - Windows console events (SetConsoleCtrlHandler)
    - `QConsoleListener` - stdin-базиран quit/exit handler (крос-платформен, в Daqster приложението)
  - **Process Management Layer** (`frame_work/base/src/process/`):
    - `QProcessManager` - generic базов клас за управление на child процеси
    - Handle-based process tracking
    - Graceful terminate с force-kill fallback
    - Virtual hooks за customization (setupProcessEnvironment, onAllProcessesFinished)
  - **ApplicationsManager Refactoring**:
    - Наследява от `Daqster::QProcessManager`
    - Backward compatibility запазена (type aliases, event mappings)
    - Daqster-specific environment setup (plugin paths, AppImage detection, XDG directories)
    - Signal forwarding (ProcessEvent → ApplicationEvent)
  - **Cross-platform Support**:
    - Платформено-независим shutdown механизъм
    - Правилно Ctrl+C handling на Windows и Unix
    - Graceful процес терминация на всички платформи
- **PluginDependencyManager System** - автоматична система за управление на plugin dependencies
  - `cmake/PluginDependencyManager.cmake` - основна система за dependency management
  - `cmake/PluginExamples.cmake` - примери за използване на системата
  - `docs/PluginDependencyManagement.md` - подробна документация
- **Automatic Plugin Management**:
  - Автоматично откриване на Qt модули, external библиотеки и packages
  - Условно компилиране на plugins според наличните dependencies
  - Подробна debug информация за plugin статус
  - Поддръжка за Qt5 (пълна функционалност) и Qt6 (ограничена функционалност)
- **External Library Integration**:
  - Qt5: NodeEditor + QtRest библиотеки включени
  - Qt6: External библиотеки изключени заради compatibility проблеми
- **Enhanced Build System**:
  - `register_plugin()` функция за лесно регистриране на plugins
  - Автоматично проверяване на dependencies
  - Условно включване на plugin subdirectories
  - Build configuration и plugin status summaries
- **NodeEditorWidget Shared Component** (`src/plugins/node_editor/node_editor_ide/NodeEditorWidget.{h,cpp}`):
  - `NodeEditorWidget` - споделен Qt Widgets GUI за node-based редактори (използван от Node Editor IDE)
  - `ChatGraphModel` - loop-enabled графичен модел
  - Автоматична инжекция на стандартните Daqster ноди (Audio, Media, Graphs, AI, etc.)
- **NodeEditorIDE Plugin** (`src/plugins/node_editor/node_editor_ide/`):
  - Базов графичен плъгин използващ `NodeEditorWidget`
  - `APPLICATION_PLUGIN` тип с `create_plugin()` макро
- **ChatGraphModel в споделената библиотека** — преместен от `node_editor_ide/` в `BuiltInNodes/Library/types/` за generality
- **REQ-SW-PL-020** (zero-copy видео дисплей):
  - `VideoFrameData` — zero-copy споделен data type (QVideoFrame Qt6 / QImage Qt5)
  - Dual-output source nodes (Qt6): `CameraSourceNode` и `VideoFileSourceNode` изпращат `VideoFrameData` (zero-copy) + `ImageData` (legacy)
  - `VideoOutputNode` — Qt6 GPU дисплей чрез detached `QVideoWidget`; Qt5 fallback към `QLabel` pixmap
  - `VideoCompat` — Qt5/Qt6 мултиплекс abstraction helpers (presentFrameCompat, presentImageCompat, connectPlayerError, connectCameraError, variantToInt, QOverload)
  - Windows cross-platform compliance (QStandardPaths, без Linux-only пътища)
  - Комити: `b5c9651` (req), `157f34d` (VideoFrameData+shim), `085f63d` (VideoOutputNode), `0f92a9c` (CMake), `c873b43` (docs), `9d0f178` (AC/status)
  - Статус: ACTIVE (impl готов, unit тестове отложени)
- (REQ-SW-PL-020) VideoFrameData и VideoOutputNode unit тестове (PL-020 AC6) — e054396.
- **REQ-SW-PL-027** (Video Pipeline Instrumentation & Overlay — първи консуматор на REQ-SW-FW-008):
  - Domain getters: `Daqster::Perf::Domain::{avg,min,max,count}(stage)` (read-only, -1 при липса на samples) в `PerfProfiler.{h,cpp}`
  - Source инструментиране: `StreamSourceNode`, `VideoFileSourceNode`, `CameraSourceNode` записват inter-frame gap (`"source.frame_interval"`) + wrap/emit сегмент (`"source.wrap_emit"`) и latch-ват `handleType`/`pixelFormat` (HW/SW маркери) в домейн `"video"`
  - Output инструментиране: `VideoOutputNode` записва `"output.present"` (blit) + `"output.total"` и latch-ва маркерите от incoming кадър
  - Qt6-only Overlay + чекбокс: „Perf" чекбокс → `Domain::get("video").setEnabled()` + 500 ms `QTimer` + полупрозрачен `QLabel` бейдж (child на detached `QVideoWidget`, горе-ляво): `HW|SW · fmt=… · handle=… · fps=… · gap=…ms · present=…ms · total=…ms`
  - `VideoPerfBadge.{h,cpp}` — pure QtCore-only `formatPerfBadge()` форматър (без widgets/QtMultimedia) + детерминистичен unit тест (8 slots)
  - Премахнат временният `VideoDiag` диагностичен блок; нулев разход при `DAQSTER_ENABLE_PERF=OFF`
  - Комити: `885ad11`, `5c0c831`, `7682bb1`, `3ecd5c0`
- **REQ-SW-PL-027 (периодичен конзолен ред + self-CPU)** — copy-paste-ващи перф числа на Qt5 + Qt6:
  - `ProcessCpu` (`src/frame_work/base/src/perf/ProcessCpu.{h,cpp}`) — self-CPU семплър за текущия процес (Linux `/proc/self/stat` utime+stime в clock ticks / Windows `GetProcessTimes()` KernelTime+UserTime); първият sample задава базовата линия и връща 0.0
  - Периодичен 5 s конзолен лог в `VideoOutputNode` (Qt5 + Qt6): `[PERF] video | SW | fmt=NV12 | handle=NoHandle | fps=30 | gap=…ms | present=…ms | total=…ms | cpu=…%` през `qCDebug(lcPerf)` (стабилен `[PERF] video` префикс за grep/copy-paste); чекбоксът „Perf" вече е и на двете Qt версии
  - Qt5 инструментиране на display пътя (`"output.present"` около `updateDisplay()`, `"output.total"` около целия image клон) + Qt5 инструментиране на source нодовете (`"source.frame_interval"` gap + `"source.wrap_emit"` около QVideoFrame→QImage конверсията)
  - `formatPerfLine()` pure QtCore-only форматър в `VideoPerfBadge.{h,cpp}` + детерминистични unit тестове (5 нови slot-а, общо 15 в `TestVideoPerfBadge`)
  - Комити: `0eb005b`, `7a71e12`, `fb0455d`, `952e416`
- **REQ-SW-PL-022** (unified sampled data transport & DAQ Display):
  - `SampledData` — единен NodeData тип (`{"sample","Sample"}`) в `src/plugins/common/NodeDataTypes/`, QtCore-only, с QByteArray + per-channel `{name, SampleType}` + double sampleRate + `decodeToNormalized()`; `AudioData` = SampledData с `domain="audio"` (без отделен клас)
  - `SampledStreamDescriptor` — консолидация на `GenericStreamConfig` + `QDevIOStreamConfig`: разширен `SampleType` enum (int8/uint8/int16/uint16/int24/uint24/int32/uint32/float32/float64), endianness, unit + amplitudeScale + amplitudeOffset, `domain` поле ("audio"/"vibration"/"daq"/"ecg"/…), device id/source name, first-sample timestamp
  - Unified decoder конвенция — signed/unsigned делят на `(2^(bits-1) − 1)` със clamp в [-1,1], floats се clamp-ват (fix на старата GenericNumericTypes 32768/no-clamp конвенция); `AudioFrameDecoder` получи QtCore-only `configure(SampleType, bits, endian)` overload
  - SampledData аудио изходен порт на `VideoFileSourceNode` + `StreamSourceNode` (append-нат ПОСЪДЪН — Qt6 порт 2, Qt5 порт 1 — старите saved graphs запазват индексите); capture: Qt6 `QAudioBufferOutput` (6.8+) / Qt5 `QAudioProbe`; буферите се обвиват в `shared_ptr<SampledData>` и се емитират гейтнати на connection count
  - **DAQ Display нод** — `Displays/DaqDisplay/DaqDisplayNode.{h,cpp}`: реален Qt Charts waveform + FFT за всякarn плъгин с sampled данни (audio/DAQ/sензори); plot слотове `std::function<QVector<float>(const SampledData&)>` (JIT-ready), v1 built-ins identity + FFT; `GenericDisplayNode` е legacy alias, `AudioDisplayModel::configureAudioView` no-op заменен с реален UI config
  - Комити: `3929326` (req), `12076a2` (Qt6 audio fix), `cf5d0ae` (SampledData+descriptor), `6395220` (decoder convention), `75e291c` (audio port), `c5559b0` (DAQ Display), `8c56e83` (Qt5/Qt6 build fixes)
  - Статус: ACTIVE (impl готов, unit тестове отложени)
- **REQ-SW-PL-023** (DaqDisplayNode Multi-Plot v1):
  - `DaqDisplayNode` — плъгин за мултиплейт plotting чрез Qt Charts, FFT за всякarn sampled источник (audio/DAQ/sензори), per-plot channel/type, off-GUI compute via `QThreadPool`, save/restore via `QDataStream`
  - `FftUtil` — утилитарна библиотека за FFT-раундлайн, `magnitudeSpectrum`, `decodeToNormalizedF32`
  - Комити: `4ba278b` (feat: DAQ Display Multi-Plot v1), `c74e7e3` (feat: FftUtil magnitudeSpectrum + SampledData decodeToNormalizedF32), `1ffac96` (chore: remove temporary thread-identity logging after Phase 4 verification), `2174008` (test: SampledData decode), `0655401` (test: FftUtil + AudioBufferToSampled)
  - Статус: ACTIVE (impl готов, unit тестове добавени и минаващи — Qt5/Qt6)
- **REQ-SW-PL-024** (AudioSource миграция от QDevIO към SampledData — нов нод
  взема името, старият → `_obsolete`):
  - Нов `AudioSourceDataModel` (registered name `AudioSource`) — SampledData поток
    `{"sample","Sample"}`; каптурата е в **dedicated worker thread**
    (`MicCaptureWorker`, moveToThread в модел-притежаван `QThread`); GUI нишката
    само keep-latest + `dataUpdated(0)`; connection-count gating — без свързан
    изход worker-ът дренира и не wrap-ва
  - Старият QDevIO mic → `AudioSourceDataModelObsolete` (registered
    `AudioSourceObsolete`, caption "(obsolete)") + `AudioWorkerObsolete` +
    `AudioNodeQdevIoConnectorObsolete` + `EventThreadPullObsolete` — rename-only,
    работи (база за benchmarking старо vs ново)
  - `AudioSourceDataModelUI` споделен и непроменен; enum `StartStop` е дефиниран
    в UI header-а (общ contract за двата нода)
  - Комити: `6ee9d3b` (refactor: rename old QDevIO mic + helpers),
    `42eb5fa` (feat: SampledData node + MicCaptureWorker), `0655401`
    (test: AudioBufferToSampled glue)
  - Статус: ACTIVE (impl готов, unit тестове добавени и минаващи — Qt5/Qt6);
    Qt5 (5.15.2) + Qt6
    (6.9.2) builds PASS + headless smoke PASS (capture в worker нишка → queued
    SampledData на GUI нишката, чисто start/stop, чисто унищожаване)
- **REQ-SW-PL-025** (DaqDisplayNode Multi-Plot v2 — физически decode + unit оси + ring buffer):
  - `SampledData::decodeToPhysical()` — header-only физически decode
    (`raw × amplitudeScale + amplitudeOffset`, БЕЗ normalization/центриране/clamp)
    + нови raw decoders в `SampledDecoder` (`rawS8..rawF64`, `decodeRawSample`);
    `decodeToNormalizedF32` остава непроменен
  - Unit оси — per-card `QValueAxis` заглавия от дескриптора: Time Domain →
    X `"Time (s)"` / Y `descriptor.unit` (fallback "normalized"/"amplitude");
    Frequency → X `"Frequency (Hz)"` / Y unit (или "Magnitude" в normalized
    режим); physical Y-обхват от min/max на декодираните стойности с ~5%
    padding (normalized картите запазват [-1, 1]); `mode` (normalized/physical)
    + `unitAxes` per-card, default за нови карти от `unit != "normalized"`
  - Worker-притежаван ring buffer — N-секундна плъзгаща се история на канал
    (raw bytes, default 10 s, `ringSeconds`); append на всеки compute pass,
    descriptor-change reset при sampleRate/канали/bytesPerFrame промяна, FFT от
    опашката (tail ≤4096), Time Domain показва целия прозорец (≤2000 точки);
    всичката работа на worker нишката (QThreadPool maxThreadCount=1) — GUI
    нишката само `series->replace()` + `axis->setRange()`
  - save()/restore() — нови опционални полета `ringSeconds`, per-card
    `mode` + `unitAxes` с defaults (10.0 / "normalized" / true) — старите v1
    файлове се зареждат без промяна
  - Комити: `6c20617` (feat: DaqDisplayNode v2 — decodeToPhysical, unit axes,
    worker-owned ring buffer)
  - Статус: ACTIVE (impl готов, unit тестове отложени); Qt5 (5.15.2) + Qt6
    (6.9.2) builds PASS + ctest 6/6 green (и двете) + offscreen smoke PASS
    (int16 32767 → 32.767, float passthrough без clamp, unit axis заглавия,
    save/restore round-trip с v1 defaults)
- **REQ-SW-FW-008** (Lightweight Runtime Profiling Framework — нов framework-level
  модул `src/frame_work/base/src/perf/` в `namespace Daqster::Perf`):
  - `RollingStats` — фиксиран pre-allocated ring buffer (O(1) `add`, без heap
    алокация в hot path); `avg`/`min`/`max` on demand, `count` капнат на
    capacity, `reset()`
  - `Domain` — именуван runtime-toggleable профилиращ домейн: thread-safe
    `get(name)` get-or-create регистър, relaxed-atomic `enabled()`/`setEnabled()`,
    `record(stage, ns)` (no-op при изключен домейн), `flush()` (агрегира през
    `qCDebug(lcPerf)` + reset)
  - `Scope` — RAII таймер за синхронни блокове (нулев разход при изключен
    домейн); `Stopwatch` — `mark()`/`reset()` за async/event мерене (ns)
  - Макрота `PERF_SCOPE(dom, stage)` / `PERF_ENABLED(dom)` с двустепенно
    изключване: compile-time (`DAQSTER_ENABLE_PERF` CMake option, default ON →
    `((void)0)`/`false` при OFF, нула Perf символи в потребителите) + runtime
    (atomic flag live toggle)
  - Нова лог категория `lcPerf` = `"daqster.perf"` в `LogCategories.{h,cpp}`
  - Unit тестове `tests/framework/perf/perf_profiler_tests` **19/19** (RollingStats
    6, Domain 7, Stopwatch/Scope 6); Qt5 (5.15.2) + Qt6 (6.9.2) builds PASS +
    ctest 9/9 green (и двете) + `-DDAQSTER_ENABLE_PERF=OFF` build PASS
  - Комити: `6ad8e89` (PerfProfiler module), `2c31b4a` (lcPerf + CMake),
    `52a620c` (perf_profiler_tests)
- **GL blit display widget** (`VideoGLBlitWidget`, `DAQSTER_GL_BLIT=1`) — detached OpenGL display за `VideoOutputNode`: качва декодираните CPU кадри като YUV текстури (NV12 / YUV420P) и конвертира в RGB във fragment shader (QImage fallback за RGB формати). И на двете Qt версии; измерено CPU: Qt5 27.9% → 15.0-16.2%, Qt6 17.8-18.2%, GLBLIT ~50-330 µs, failures=0.
- **NV12-direct за Qt5 (REQ-SW-PL-020)** — `VideoFrameData` вече не е Qt6-gated; Qt5 source-ите емитират OWNED копие на декодирания кадър (`VideoCompat::frameToOwnedFrame` — NV12/YUV420P plane-ове memcpy в `QAbstractPlanarVideoBuffer`) на port 0 (video-frame), image port 1 конвертира on-demand, audio port 2 appended last. `VideoOutputNode` има dual input video-frame@0 / image@1 и на двете версии. **Преномерация:** на Qt5 image портът се мести от 0 на 1 — стари Qt5 графи с image@0 трябва да се пресвържат.
- **Perf резултати doc** — `tests/performance/performance-video-display-2026-08-13.md`: пълна методология, числа преди/след shadow, perf анализ, промени и следващи лостове.
- **Qt6 in-scene GPU video display (REQ-SW-PL-021)** — чекбоксът „GPU display" вече е **видим и на Qt6** (checked по подразбиране = detached `QVideoWidget`, запазено текущото поведение). При unchecked `VideoOutputNode` създава `QGraphicsVideoItem` като child на `NodeGraphicsObject`-а на node-а (`ensureSceneVideoItem()` — намира сцената през `QApplication::topLevelWidgets()` → `GraphicsView` → `DataFlowGraphicsScene`) и кадрите се подават през `VideoCompat::presentFrame(item->videoSink(), frame)` — GPU път без QImage копие; софтуерният QLabel път остава автоматичен fallback. Detached прозорците се затварят при unchecked; in-scene item-ът се трие при toggle/дисконект. Perf бейджът следва само detached прозорците. Qt5 пътищата са непроменени. Dev driver: `DAQSTER_SCENE_VIDEO=1` uncheck-ва чекбокса за headless проверка. Комити: `63c7f78`, `a04f05e`, `3a0686e`, `ba7561f`. Smoke: Qt6 in-scene (кадрите се рендерират в сцената) + Qt6 detached + Qt5 GL blit regression — PASS без crash; ctest 9/9 (Qt5 + Qt6).
- **REQ-SW-PL-034** (VideoOutputNode embedded effects — optional, default none):
  - `VideoOutputNode` получава **опционални вградени ефекти** — същите 11 ефекта като `VideoEffectNode` (brightness, contrast, grayscale, invert, sepia, channelSwap, flip, blur + gaussianBlur/canny/threshold при `HAVE_OPENCV`), **без ефект по подразбиране**
  - Комбобокс с водещ „No effect" (index 0) + `QStackedWidget` с параметърни страници (slider/flip/OpenCV); backend суфикс „(GPU)"/„(CPU)" като `VideoEffectNode`
  - Когато ефект не е избран, блокът в `setInData()` се **пропуска изцяло** — `asTexture()`/`asImage()` не се викат, zero-copy passthrough-ът е byte-identical (AC 2)
  - GPU path: `asTexture()` → `VideoEffectGLProcessor::processTexture()` → `fromTexture()` (GpuRgba, zero-copy) при хардуерен GL; CPU path/fallback: `asImage()` → `EffectSpec::cpuApply` → `QVideoFrame`
  - `save()`/`load()`: `"effect"` id + параметри (`brightness`/`contrast`/`flipMode`/`blurRadius`/`gaussianKernel`/`cannyLow`/`cannyHigh`/`thresholdValue`) с clamp-ове; стари графи без `"effect"` ключ → „no effect" (backward compatible)
  - Тестове: `demo_nodeditor_videooutput_tests` +4 (default no-effect passthrough, load→save round-trip, backward compat, ефектът трансформира frame-а); Qt5/Qt6 builds PASS + ctest 9/9 green (и двете)

### Changed
- **GpuRgba кадри → GL blit widget на Qt6 (REQ-SW-PL-032, Stage 2C, perf)** — `VideoOutputNode` маршрутизира GpuRgba кадри (ефект изходи) към GL blit widget-а (`presentTexture`, zero-copy bind, ~19 µs/display) и на Qt6, когато hardware GL е наличен (`VideoGLContextManager::hasHardwareGL()`) — вместо native `QVideoWidget` пътя (readback `glReadPixels` 8 MB + per-sink RHI upload 8 MB/display; 2 display-а на ефект изход → CPU 10% → 50%+). CPU/NV12 кадрите запазват per-Qt backend-а (Qt6: native `QVideoWidget`, Qt5: GL blit). `ensureVideoWidget(wantGlBlit)` избира backend-а per-frame и унищожава другия widget при превключване (без leak, без crash).
- **Node body repaint on data arrival is now opt-out (REQ-SW-PL-032, perf)** — `dataArrivalChangesWidget()` virtual в nodeeditor: video/LLM/console нодовете (`VideoOutputNode`, `VideoEffectNode`, `CustomShaderNode`, `FrameSamplerNode`, `LLamaModelDataModel`, `ConsoleDataModel`) връщат `false` и пропускат per-frame `node->update()` repaint-а на тялото на нода (widget съдържанието се self-repaint-ва през Qt); нодовете с validation state (NumberDisplay, DaqDisplay, ...) запазват default-а `true`. nodeeditor комит: `43c1dea`
- **Validation nodes opt out + self-invalidate (REQ-SW-PL-032, perf)** — `setValidationState()` в nodeeditor вече емитира `requestNodeUpdate()` (validation border-ът се self-repaint-ва само при реална промяна на state-а); `NumberDisplay`, `DaqDisplay`, `GenericDisplay`, `Modulo`, `ArithmeticLogic` връщат `dataArrivalChangesWidget() == false`. nodeeditor комит: `566f6ba`
- **NumberDisplay geometry само при реален resize (REQ-SW-PL-032, perf)** — `dataArrivalChangesGeometry() == false` + event filter на `m_wrapper`: `requestNodeUpdate()` (recomputeSize + moveConnections) се емитира само при `QEvent::Resize`, не на всеки data arrival; `recomputeSize()` само чете widget size-а → без рекурсия
- **VideoOutputNode output порт — zero-copy passthrough (REQ-SW-PL-032, fix)** — при свързан downstream consumer output-ът е `m_output = videoFrame` (същият shared `VideoFrameData`, residency-то се запазва) — без QImage readback, без `QVideoFrame` re-wrap, без double-present (`updateDisplay()` махнат от блока)
- **`VideoGLBlitWidget::presentYuvTexture()` (REQ-SW-PL-032, perf)** — представя кешираните YUV текстури от `asTexture()` директно (NV12/YUV420P програми, стандартен top-down quad) — без дублиращ upload; fallback към `presentFrame()` при invalid handle
- **`VideoFrameData` copy ctor/assignment = delete (REQ-SW-PL-032, fix)** — frame-ите се споделят само през `shared_ptr`, никога не се копират (GpuRgba копията бяха задънена улица); move семантика не е била налична
- **`VideoEffectGLProcessor::process()` премахнат (REQ-SW-PL-032, refactor)** — dead code (FBO readback trap): няма callers, само `processTexture()` се ползва; `uploadFrame()`/`classifyYuv()`/текстурите `m_texY/U/V/UV`/`m_useNv12` също са махнати
- **Design refinement (REQ-SW-PL-028/029/032, 2026-08-25)** — documentation-only:
  - PL-028: рефакторинг към **ЕДИН** `VideoEffectNode` с комбобокс за избор на ефект + параметри/конфигурация (вместо 7 отделни subclass-а); EffectSpec-ите остават в `VideoEffectOps.h/.cpp`
  - PL-032: уточнен дизайн — lazy кешове (`asImage()`/`asTexture()`), node residency предпочитания (Вариант C), GPU-resident транспорт (Път B), Qt6-първо/Qt5-после, формат NV12 → Y+UV → YUV→RGB+ефект → RGBA
  - PL-029: потвърден отделен нод (не вграден в `VideoEffectNode`) — различно UI (GLSL редактор + compile + error log), разширяем към DAQ/други типове
- **VideoEffectNode комбобокс рефакторинг (REQ-SW-PL-028 AC 4/5, имплементиран)** — 7-те per-effect subclass-а стават deprecated aliases; един `VideoEffectNode` с комбо + `QStackedWidget` (7 страници: brightness slider, contrast slider, grayscale/invert/sepia/channelSwap info, flip combo). `setEffect(id)` избира по id (непознат → индекс 0); `save()`/`load()` форматът е непроменен (`"effect"` = id + параметри) — backward compatible. Smoke driver-ът (`DAQSTER_AUTOSTART_EFFECT`) добавя един `VideoEffect` нод и задава ефекта през `load()`. Комити: `e84f6d0`, `42bb57a`, `9fb46b9`
- **VideoFrameData lazy QImage кеш (REQ-SW-PL-032 AC 1/2/3 CPU част, имплементиран)** — `asImage()` конвертира веднъж на кадър и кешира; `setFrame()` чисти кеша; консуматорите (`VideoEffectNode` CPU път, `VideoOutputNode` software/consumer път) ползват кеша вместо `VideoCompat::frameToImage`. Комити: `3f9ec84`, `093b557`
- **GPU-resident транспорт Stage 2A (REQ-SW-PL-032 AC 1/5, имплементиран)** — `VideoGLContextManager` (споделен GL контекст, share group с display-а), `VideoFrameData::asTexture()` (lazy Y/U/V upload, кеширан handle) + `fromTexture()` (GPU-resident RGBA wrap) + `isGpuResident()`; `VideoEffectGLProcessor` мигрира към споделения контекст. Комити: `6cb1aaa`, `803e4f6`, `9a39006`
- **GPU-resident ефект верига Stage 2B (REQ-SW-PL-032 AC 5/7, имплементиран за ефект веригата)** — `VideoEffectGLProcessor::processTexture()` (текстура-вход → текстура-изход, без `toImage()` readback; program cache key с layout ∈ {nv12, 420p, rgba}; нова `buildRgbaEffectFragmentSource()`); `VideoEffectNode` GPU път `asTexture()` → `processTexture()` → `fromTexture()` — вторият ефект консумира текстурата директно; `VideoGLBlitWidget::presentTexture()` — zero-copy display на RGBA текстурата; Qt6 native display прави readback на границата (Stage 2C). **PERF (Qt5 GL blit, sepia): cpu 62.2% → 16.9%, present 7.1ms → 0.1ms, GLBLIT avg 322us → 18.8us (fmt=Texture(RGBA)); 2-ефекта (sepia+invert) cpu 19.0%; Qt6: 34% → 25% (1 ефект), 26.8% (2 ефекта).** Комити: `b6af20e`, `d1a5e7a`
- **Фаза 3 — миграция към един VideoFrameData тип (REQ-SW-PL-032, 2026-08-26)**:
  - **Image портовете премахнати** от video source-ите (`CameraSourceNode`, `VideoFileSourceNode`, `StreamSourceNode`) — емитират само `VideoFrameData` (port 0) + `SampledData` (port 1, audio); `VideoOutputNode` приема само `VideoFrameData`. Комити: `63010f3`, `3fc51a3`
  - **`VideoTransformNode` премахнат** — заменен от `VideoEffectNode`, който покрива всичките му операции (вкл. blur/OpenCV). `VideoTransformOps.{h,cpp}` + `OpenCVTransforms.cpp` остават (ползвани от `VideoEffectOps`). Комит: `688c899`
  - **`ImageData` типът изтрит** — единственият frame тип е `VideoFrameData`; grep `ImageData` в `src/` и `tests/` → 0. Комит: `817002e`
  - **Saved-graph последици:** стари графи с `"VideoTransform"` registry ключ или image edges няма да се заредят — пресвържете към `VideoEffect` + `VideoFrameData` вериги (документирано в README-а)
- **CI overhaul (Qt6 primary)** — `.github/workflows/ci.yml` пренаписан: Qt6 е PRIMARY (Linux + Windows), Qt5 е compat; добавени ctest (offscreen), AppImage smoke test, submodule reachability check, windeployqt вместо ръчен DLL copy; премахнати dead Debug стъпки и choco ninja
- **Release workflow** — `.github/workflows/release.yml`: Qt6 builds, tarball + ZIP + SHA256SUMS, release body от CHANGELOG секцията, smoke tests
- **AppImage packaging** — `tools/create_appimage.sh`: филтриране на test/private plugins, GStreamer backends bundled, Qt5/Qt6-aware Qt copy
- **GitHub Pages** — сайтът ползва UI-based deployment (Settings → Pages → Deploy from a branch: `master/docs`); `.github/workflows/pages.yml` е **изключен** (`pages.yml.disabled`, като DeepSource) — workflow не е нужен, опционален/re-enableable
- **Docs rename fix** — INDEX.md → index.md references актуализирани
- **Directory Restructuring**:
  - `src/external_libs/` → `src/plugins/external_libs/` (всички external libs са под plugins)
  - `.gitmodules` paths актуализирани
- **nodeeditor target**: Променен от `nodes` на `QtNodes` (pin commit `4709573`)
- **cmake/ComponentTemplates.cmake**: `create_external_library()` path → `src/plugins/external_libs/`
- **CI Workflow**: Добавени Python patch стъпки за qtrest install fix (cmake_install.cmake patching) — вече не са нужни, upstream install правилата са поправени (вж. `docs/operations/UpstreamManagement.md`)
- **Architecture docs** актуализирани за новата структура
- **Documentation**:
  - Plugins hub (`docs/plugins/README.md`) + fix на plugin documentation links в INDEX/Architecture (`b5c204f`)
  - Demo plugin README — документирани video nodes и optional OpenCV (`e2c4925`)
  - REQ-SW-PL-018/PL-019 documentation refs backfill; plugin version alignment 0.3.0 → 0.2.0 в `project()` за `demo_nodeditor_nodes` и `node_editor_ide` (inert metadata, съответства на runtime 0.2.0) (`ed8b334`)
- **StreamUrlValidator** — извлечен от `StreamSourceNode` като самостоятелен header-only helper (`3048fbd`) за unit-testability (валидация на http/https/rtsp stream URL)
- **Process**: mandatory branch-per-work-item clause в AGENTS.md (`bee28c4`); trunk-based-lite from master (`547401c`); master consolidation (PR #21 `3c8c47f` merged, phase3 branch изтрит, PL-020 rebased onto master)
- **Node drop shadows изключени (perf)** — nodeeditor default `DefaultStyle.json` вече е с `ShadowEnabled=false` и display нодовете (`VideoOutputNode`, `DaqDisplayNode`, `NumberDisplayDataModel`, `QDevIoDisplayModelObsolete` вкл. AudioDisplay) го форсират per-node. `QGraphicsDropShadowEffect` blur-ът се изпълнява при всеки scene repaint и струваше ~46% CPU при video playback (измерено Qt6 36% → 17.6%).
- **GL blit става DEFAULT за Qt5 (REQ-SW-PL-021)** — `VideoOutputNode` избира display backend-а така: Qt5 = GL blit **ON** по подразбиране (най-бързият път), Qt6 = native `QVideoWidget` (GL blit OFF). `DAQSTER_GL_BLIT` е debug override само за стартиране: `=0` форсира софтуерен път (и Qt5), `=1` форсира GL (и Qt6). Поправен бъг „=0 включва": стойността вече има значение (`qEnvironmentVariableIntValue`).
- **Auto-fallback при недостъпен GL (REQ-SW-PL-021)** — при неуспех на GL контекста (`glPlatformAvailable()` pre-probe преди създаване на widget + async `QOpenGLWidget::isValid()` check след show) `VideoOutputNode` логва `GL fallback: <причина>` и превключва на софтуерния път (Qt5: кадър → QImage → QLabel; Qt6: native QVideoWidget). Видеото продължава да се показва (25 fps) без crash. Session-guard `m_glFailed` предотвратява повторно създаване на счупен прозорец.
- **UI toggle „GPU display" (REQ-SW-PL-021)** — чекбокс в `VideoOutputNode`, видим + checked по подразбиране и на двете Qt версии. Qt5: checked → detached GL blit, unchecked → софтуерен QLabel. Qt6: checked → detached прозорец (`QVideoWidget` или GL blit при `DAQSTER_GL_BLIT=1`), unchecked → in-scene `QGraphicsVideoItem` (вж. „Qt6 in-scene GPU video display" в Added). Потребителят може да го включва/изключва на живо — прилага се при следващия кадър, без crash и без загуба на видео. Приоритет: UI има последната дума след старт; env var-ът е само за стартиране.
- **Qt5 софтуерен display path за video-frame порт** — преди това Qt5 без GL blit не показваше кадрите на port 0 (video-frame); сега frameToImage → QLabel показва видеото (измеримо: `DAQSTER_GL_BLIT=0` ≈ 34-36% CPU).
- **Video source порт преномерация на Qt5** — виж NV12-direct по-горе; стари Qt5 графи, свързващи source port 0 (беше "image") с ImageData консуматор, губят връзката.

### Fixed
- **RGBA (FBO) input orientation bug в GPU ефект процесорите (REQ-SW-PL-032)** —
  `VideoEffectGLProcessor` и `CustomShaderGLProcessor` семплираха bottom-up
  FBO-произведени RGBA текстури със стандартния top-down quad → even-length
  GPU ефект вериги (2 ефекта) и единичен CustomShaderNode pass се показваха
  вертикално обърнати. Фикс: flipped-v quad (`kQuadVerticesFbo`, v' = 1 − v)
  при семплиране на RGBA input / pre-pass intermediate, огледално на display
  path-а (`VideoGLBlitWidget`). Single effect (YUV input) непроменен.
- **Per-frame texture allocation в ефект пътя (REQ-SW-PL-032 Issue #7)** —
  нов `TexturePool` (`src/plugins/common/GL/TexturePool.{h,cpp}`): `acquire(w,h)`
  reuse-ва свободна текстура (преоразмерява storage-а при смяна на
  резолюцията), `release(tex)` я връща в pool-а; `VideoFrameData::fromTexture()`
  приема опционален release callback (shared_ptr към pool-а), който връща
  текстурата при унищожаване на frame-а вместо `glDeleteTextures`. Ефект
  пътят вече няма per-frame `glGenTextures`/`glDeleteTextures`.
- **`setValidationState` re-trigger-ваше пълния път всеки кадър (nodeeditor
  submodule)** — early-return при непроменено състояние (NumberDisplay-class
  нодове викат `setValidationState` всеки кадър със същото състояние).
- **Двойна `nodeUpdated` емисия (nodeeditor submodule)** — при
  `NodeRole::ValidationState` през graph model API-то `setValidationState` →
  `requestNodeUpdate` → `nodeUpdated` + директна `nodeUpdated` емисия; директната
  емисия е премахната.
- **Write-only `m_videoFrame` в `VideoOutputNode`** — премахнат (заменен с
  `m_lastInput`, който `reprocessCurrentFrame()` ползва); `load()` вика
  `reprocessCurrentFrame()` след зареждане (консистентно с `VideoEffectNode`).
- **nPorts off-by-one на video source нодове (REQ-SW-PL-022)** — `VideoFileSourceNode` и `StreamSourceNode` имаха nPorts=2 вместо 3 (липсваше audio Sample port на индекс 2); поправен в комит `d5145c2` (`restore nPorts to 3 — audio Sample port was unreachable at index 2`); AC 8 (backward compat) засегнат и поправен — saved графове с audio port на индекс 2 вече работят коректно
- **Конзолният `quit` не работеше от main app launcher-а (REQ-SW-APP-002, PUB-002)** —
  `QConsoleListener` се създаваше само вътре в `if (args.count() > 0)` клона на
  `main()` (след plugin load), затова при стартиране на Daqster без аргументи
  (основния launcher → `QMainWindow` + toolbar) stdin `quit\n` беше игнориран и
  приложението не излизаше. Сега `QConsoleListener` се създава **безусловно**
  преди `if (args.count() > 0)` — `quit` работи на всички стартови пътища (main
  launcher, single-arg plugin, multi-arg spawner). Проверено с PTY harness (сам
  процес, без child spawn-ове, DISPLAY=:0): `quit\n` извежда процеса за
  0.12-0.17 s (Qt5 + Qt6, с и без `NodeEditorIde`), exit 0. Коммит `9893d35`.
- **QConsoleListener busy-spin при stdin EOF (REQ-SW-APP-002, PUB-002)** — при
  стартиране на Daqster без blocking stdin (`< /dev/null`, затворен stdin от
  IDE/launcher, или приключил pipe) `QSocketNotifier` на `fileno(stdin)`
  зацикляше: EOF е перманентно "readable", всяко активиране четеше празен ред
  и емитираше празно `newLine("")` → **busy-spin ~181-183% idle CPU**. Сега при
  първото празно четене (`readLine()==0` ⇒ `read()` върна 0 ⇒ EOF) notifier-ът
  се disable-ва; жив терминал/pipe с данни не се засяга (при липса на вход fd
  не е readable и notifier-ът не се активира). Windows: `QWinEventNotifier` се
  disable-ва при `getline()==""` + `std::cin.eof()`. Измерено след fix-а: idle
  CPU без blocking stdin **0.0%** (Qt6 и Qt5, instantaneous /proc stat deltas;
  lifetime avg 3.8%/3.3% вкл. startup), срещу ~181-183% преди. Workaround-ът
  `tail -f /dev/null |` вече не е нужен. Коммит `6900e2c`.
- **Plugin-not-found / create-failure startup — процесът излиза сам (REQ-SW-APP-003)** —
  при `Daqster <plugin-name>` (single-arg) с несъществуващ plugin (нито hash, нито
  name match) или при намерен hash, но неуспешен `CreatePluginObject` (nullptr),
  `main.cpp` показваше `QMessageBox::critical` и след това продължаваше към
  `a.exec()` — процесът оставаше жив като ПРАЗЕН прозорец без функционалност,
  докато не се затвори ръчно. Сега и в двата failure path-а след затварянето на
  диалога `main()` връща `1` (exit code != 0) — процесът (вкл. child процесите,
  спавнати от launcher-а през `ApplicationsManager::StartApplication`) се затваря
  сам, без празен прозорец. Success path-ът (валиден plugin) и multi-arg
  spawner-ът са непроменени. Проверено ръчно (Qt5 + Qt6, DISPLAY=:0): диалог се
  показва, след Return/OK процесът излиза с код 1; `NodeEditorIDE` стартира и
  конзолният `quit` продължава да работи (REQ-SW-APP-002).
- **Plugin launch fixes**:
  - Toolbar пуска plugins по име вместо stale hash
  - Prune на persisted plugin entries с несъответстващ файлов hash при load
  - Явна грешка (qCCritical + QMessageBox) при неуспешно зареждане на plugin
  - AppSelectionDialog съхранява visibility по plugin name
- **Video нодове** — компилация на Qt5 и Qt6 (VideoCompat helpers: connectPlayerError/connectCameraError, variantToInt за Qt5 QVariant, QOverload за error signal)
- **Dependency graph** — edges следват nodes при drag, viewport се fit-ва при resize
- **Requirements Manager** — dedup на requirement файлове, достижими през множество roots, предупреждение при добавяне на overlapping root
- **NumericType::numberAsText()** — fix за ambiguous overload при int тип: явно cast до `double` с precision 0, предотвратява показване на hex/placeholder стойности вместо числа
- **VideoTransformNode Flip (Qt5)** — вертикалният flip ползваше `QImage::mirrored()` с грешни параметри на Qt5; поправен (`0cf6d19`), открит от новите unit тестове
- **Exporter CSV header** — колона "Repo" в CSV header-а на traceability matrix експорта беше с несъответстващо име; поправен (`9d90fff`)
- **LoggingSystem.md** — symlink заменен с regular file за GitHub docs rendering (`b247046`)
- **VideoOutputNode (Qt6 GPU path)** — премахната per-frame QImage конверсия + QLabel update + ImageData output когато GPU пътят (detached QVideoWidget) е активен; QLabel-ът показва статичен placeholder ("GPU display active — see detached window") веднъж; QImage конверсията + ImageData output се изпълняват само когато има downstream consumer connected на output port 0 (проследявано чрез `outputConnectionCreated`/`outputConnectionDeleted` + `m_outputConnectionCount`). Това премахва двойната CPU работа при активен GPU път.
- **VideoOutputNode (Qt6, detached popup)** — detached QVideoWidget popup-ът вече се затваря при премахване на port-0 connection (`inputConnectionCreated`/`inputConnectionDeleted` + `m_videoInputConnected` guard в `setInData()`). Преди това disconnect не спираше source player-а и кадрите продължаваха да пристигат в `setInData()`, възкресявайки popup-а. Също: null-check на `m_videoWidget` преди `presentFrame()` и спиране на излъчването на `ImageData` с null QImage (`d86b095`)
- **Temporary Qt6 video diagnostics** — qDebug логване на `QVideoFrame` (isValid, surfaceFormat().pixelFormat, handleType, map(), toImage()) в `VideoFileSourceNode`/`StreamSourceNode` onFrameAvailable (първите 10 кадъра) + еднократен dump на първия валиден кадър в `/tmp/qt6_frame_dump.png`. ВРЕМЕНЕН код — да се премахне след диагностиката (`abd46db`)
- **Тихо аудио на Qt6 (REQ-SW-PL-022)** — `VideoFileSourceNode` и `StreamSourceNode` създаваха `QMediaPlayer` без `QAudioOutput`; Qt6 изисква `setAudioOutput()` (иначе аудиото се декодира, но не се рутира). Добавен `QAudioOutput` member + `setAudioOutput()`, гарднат с `#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)`; Qt5 поведението непроменено (`12076a2`)
- **`[PERF]` конзолен ред не излизаше (REQ-SW-PL-027)** — `VideoOutputNode::logPerfLine()` логваше през `qCDebug(lcPerf)`, но категорията `daqster.perf` е изключена по подразбиране в `LogManager` → редът се филтрираше. Преминато на `qInfo().noquote()` (Info ниво, без категория — винаги излиза, като FFmpeg `[INF]` редовете); форматът на реда (formatPerfLine) е запазен
- **Perf overlay бейджът не се виждаше (REQ-SW-PL-027, Qt6)** — бейджът беше `QLabel` child на detached `QVideoWidget`; `QVideoWidget` рендерира видеото в отделен native слой (RHI swapchain) и не композира child widget-и върху него → бейджът оставаше невидим. Мигриран към отделен top-level frameless tool прозорец (`Qt::Tool | FramelessWindowHint | WindowStaysOnTopHint`, `WA_TransparentForMouseEvents`, полупрозрачен фон), който следва позицията на видео прозореца (горе-ляво, offset ~4,4) при създаване и на всеки ~500 ms refresh; `show()`/`hide()` според `domain.enabled()`; затваря се при `inputConnectionDeleted`/деструктор
- **Detached GL blit прозорецът не се появяваше при повторно закачане (Qt5 + Qt6 image порт)** — разкачането криеше GL прозореца (null клон на `updateDisplay()`), но `ensureGlWidget()` връщаше рано при съществуващ-но-скрит widget, а Qt5 `inputConnectionDeleted()` беше no-op → прозорецът не се връщаше при повторно закачане. `ensureGlWidget()` вече re-show-ва скрит widget и Qt5 disconnect премахва прозореца като Qt6. Проверено ръчно и на двете версии.
- **Qt5 видео throttled до 1 fps при видим GL прозорец** — на NVIDIA GLX default swap interval throttled `QOpenGLWidget` repaint-ите до ~1 Hz (възпроизведено с минимален standalone widget), което задушаваше видео пайплайна до 1 fps. `VideoGLBlitWidget` задава `QSurfaceFormat::setSwapInterval(0)` (+ default format) → свободни presents → 25 fps.
- **CI round 2 — 3 failure fixes (2026-08-29)**:
  - **Linux Qt5 videooutput тест crash (signal 11)** — `TexturePool` деструктор/`acquire()` сравняваха `QOpenGLContext::currentContext()` с `mgr.context()`; при неуспешно създаване на GL контекст (headless CI без mesa) `mgr.context()` е `nullptr` и при липсващ current context сравнението дава `true`, заобикаляйки `makeCurrent()` guard-а → null deref в `context()->functions()`. Фикс: явен `mgr.context() == nullptr` guard в `TexturePool::~TexturePool()` и `TexturePool::acquire()`. CI: добавени mesa GL пакети (`libgl1-mesa-dri libegl1 libgl1 libglx-mesa0`) към Linux Qt5 и Qt6 jobs (offscreen GL via llvmpipe).
  - **Linux Qt6 build fail (Qt 6.4.2 твърде стар)** — system Qt на ubuntu-24.04 е 6.4.2, но кодът ползва `QVideoFrame(QImage)` ctor (6.8+) и `QImage::flipped()` (6.5+). Фикс: Qt 6.8.3 през `jurplel/install-qt-action@v4` (`linux/gcc_64`, modules `qtcharts qtmultimedia`; qtdeclarative/qtsvg са в base) + `-DCMAKE_PREFIX_PATH=$QT_ROOT_DIR`; `create_appimage.sh` поддържа aqtinstall layout (`lib/`, `plugins/`, bundled ICU).
  - **Windows Qt6 configure fail (VS 2022 not found)** — `-G "Visual Studio 17 2022"` не откриваше VS instance на windows-latest. Фикс: `ilammy/msvc-dev-cmd@v1` + `-G Ninja` (single-config: build/test/install без `--config`); windeployqt ползва `QT_ROOT_DIR` (install-qt-action не задава `Qt6_DIR`).

### Known issues
- **VC-1 Advanced видео на Qt6 (known issue)** — VC-1 Advanced профил съдържание (напр. `50MB_1080P_THETESTDATA.COM_AVI.avi`, ASF контейнер, mislabelled .avi) показва зелен екран на Qt6: Qt FFmpeg backend (QFFmpegMediaPlugin, libavcodec 61/FFmpeg 7.1) доставя NV12 кадри с изцяло нулева chroma плоскост (Cb=Cr=0) → YCbCr→RGB = плътен зелен (0,226,0). Доказано със standalone probe без Daqster presentation код (system FFmpeg декодира правилно; H.264 и Cinepak работят на Qt6; Qt5 работи via GStreamer). Не е fixable в Daqster код — upstream Qt 6.9.2 bug. Открито на 2026-08-09.

### Removed
- `src/plugins/node_editor/` — монолитен plugin (заменен от widget + app)
- `src/external_libs/` — празна директория премахната
- **7-те deprecated VideoEffect alias нода (REQ-SW-PL-028, 2026-08-26, решение на потребителя)** — `VideoEffectBrightnessNode`, `VideoEffectContrastNode`, `VideoEffectGrayscaleNode`, `VideoEffectInvertNode`, `VideoEffectSepiaNode`, `VideoEffectChannelSwapNode`, `VideoEffectFlipNode` премахнати от `VideoEffectNode.h` и регистрациите им от `DemoNodeEditorNodesObject.cpp`. Единственият регистриран ефект нод е `VideoEffect` (с комбобокс). **Стари saved графи, които реферират alias registry ключовете, вече няма да се зареждат** (прието последствие).
- **`VideoTransformNode` (REQ-SW-PL-032 Фаза 3, 2026-08-26)** — премахнат от регистрацията, CMake и файловата система; заменен от `VideoEffectNode` (покрива всичките му операции вкл. blur/OpenCV). `VideoTransformOps.{h,cpp}` + `OpenCVTransforms.cpp` остават (ползвани от `VideoEffectOps`). Комит: `688c899`
- **`ImageData` тип (REQ-SW-PL-032 Фаза 3, 2026-08-26)** — `src/plugins/common/NodeDataTypes/ImageData.h` изтрит; единственият frame тип е `VideoFrameData`. Комит: `817002e`
- **Image портове (REQ-SW-PL-032 Фаза 3, 2026-08-26)** — премахнати от video source-ите и `VideoOutputNode`; source-ите емитират само `VideoFrameData` + `SampledData` (audio). Комити: `63010f3`, `3fc51a3`

## [0.2.0] - 2025-09-18

### Added
- **Unified AppImage build system** - единен скрипт `tools/create_appimage.sh` за локално и CI създаване на AppImage
- **GitHub Actions CI/CD** - автоматизирани билдове и releases с AppImage артефакти
- **Debug AppImage support** - отделни Debug и Release AppImage билдове
- **Comprehensive documentation**:
  - `docs/Architecture.md` и `docs/Architecture.en.md` с PlantUML диаграми
  - `docs/HowToDebugAppImage.md` - ръководство за дебъгване на AppImage
  - Обновени README файлове с подробни инструкции за билдове и environment променливи
- **Enhanced plugin system**:
  - Подобрено environment variable handling за child processes
  - Debug output за plugin discovery paths
  - AppImage detection и адаптивно поведение
- **Professional build types** - Debug (за разработка) и Release (за production)
- **Environment variables documentation** - пълно описание на всички променливи за plugin discovery, Qt environment и debugging

### Changed
- **AppImage creation process** - автоматизиран с unified скрипт, поддържа local и CI режими
- **Plugin launching** - подобрено за AppImage среда с правилно environment setup
- **CI/CD workflows** - опростени и оптимизирани за AppImage създаване
- **Documentation structure** - организирана в `docs/` директория с PlantUML диаграми

### Fixed
- **Plugin discovery в AppImage** - поправени environment variables за child processes
- **QML loading issues** - поправен QtCoinTrader plugin за правилно QML зареждане
- **CI permissions** - поправени execute permissions за AppImage скриптове
- **Plugin launching от меню** - работи правилно в AppImage среда

### Technical Details
- **Build system**: CMake 3.16+, Qt 5.15.2, AppImage packaging
- **Plugin architecture**: Dynamic loading с hash-based deduplication
- **Process isolation**: Plugins се стартират като отделни QProcess
- **Cross-platform**: Linux AppImage distribution готов

## [0.1.0] - 2025-08-29
### Added
- Единен CMake билд: top-level `CMakeLists.txt` и поддиректории за apps, frame_work, plugins, external_libs.
- `install()` цели за `Daqster`, `frame_work`, `NodeEditorPlugin`, `QtCoinTraderPlugin`.
- RPATH настройки за runtime намиране на библиотеки (`$ORIGIN/../lib`).
- **Professional plugin discovery system** с приоритетни пътища:
  - Build директория (най-висок приоритет за дебъг)
  - Environment variables: `DAQSTER_PLUGIN_DIR` (една директория) и `DAQSTER_PLUGIN_PATH` (множество директории)
  - User plugins: `~/.local/share/daqster/plugins`
  - System plugins: `/usr/lib/daqster/plugins` и `/usr/local/lib/daqster/plugins`
- **Hash-based plugin deduplication** - предотвратява дублиране на същите plugins в различни директории.
- **AppImage-ready config system** - config файл се създава в writable location (`~/.config/Daqster/daqster.ini`).
- README на български и отделно `README.en.md` на английски с подробни инструкции за plugin discovery.

### Changed
- Премахнати всички qmake `.pro` файлове.
- Обновен `README.md` към CMake-only инструкции.
- **Config файл location** - от build директория към writable location за AppImage compatibility.

### Notes
- Начална публична версия, подготвена за CI/CD настройка.
- **Professional plugin architecture** готова за distribution като AppImage или Flatpak.

[0.1.0]: https://github.com/samiavasil/Daqster/releases/tag/v0.1.0


