# LocalServerService — един supervisor за локалните сървъри (as-built)

Родител: [Архитектура](./README.md) · REQ-SW-PL-081

## Какво прави и къде живее

`LocalServerService` управлява **живота на произволен локален сървър по HTTP** —
нищо повече. Един цикъл, един и същ за всеки сървър:

```
свободен порт → стартиране → GET <healthPath> до 200 → файл със собствеността
              → (работа)  → ограничен стоп
```

| Какво | Къде |
|---|---|
| класът | `src/plugins/common/Servers/LocalServerService.h` → `Daqster::LocalServerService` |
| описанието на това какво се пуска | `Daqster::LocalServerSpec` (същият header) |
| имплементацията | `src/plugins/common/Servers/LocalServerService.cpp` |
| общата библиотека | `DaqsterCommonLibrary` (`src/plugins/common/CMakeLists.txt`) |
| тестовете | `tests/plugins/common/test_local_server_service.{h,cpp}` + `local_server_stub.cpp` |
| лог категорията | `daqster.threading` (`Threading/ThreadingLog.h` — един и същ въпрос: чий е този дъщерен процес и кога изчезва) |

Състояние: `State{Stopped, Starting, Ready, Failed}`. API: `start()`, `stop()`,
`isReady()`/`isOwned()`/`isReused()`, `port()`/`baseUrl()`, `state()`,
`lastError()`. Сигнали: `started(int port)`, `failed(QString reason)`,
`logLine(QString)`.

`LocalServerSpec` съдържа само неща за живота на процеса: `kind`, `binary`,
`arguments` (**без** `--host`/`--port`), `host`, `preferredPort`,
`portRangeStart/End`, `healthPath`, `readyTimeoutMs`, `healthIntervalMs`,
`stopGraceMs`, `stopKillMs`. Портът се добавя от класа (`launch()`), за да не
може викащият да загуби порта, на който после ще пита `/health`.

**Защо е framework примитив, а не част от плъгин.** Животът на локален сървър не
съдържа нито един ред, специфичен за предметната област: порт, `/health`, PID,
собственост. Всеки плъгин, който пуска локален сървър (inference, база данни,
локален демон), има точно този проблем. Държавен в частния плъгин, кодът се
копира при първия друг потребител — затова живее до `Threading/` и се ползва
оттам. Класът **не знае** кой е сървърът: `kind` е данни, не име на файл.

## Две състояния на собственост (не се смесват)

| | `own-runner` | `external` |
|---|---|---|
| Флаг | `isOwned() == true` (`isReused() == false`) | `isReused() == true` (`isOwned() == false`) |
| Как се стига | никой не отговаря на well-known порта → `launch()` | `GET /health` върна `200`/`503` → `adoptExistingServer()` |
| Процес | `m_procOwner` (`Daqster::ProcessOwner`) | няма наш процес; знаем само порта |
| `stop()` | `ProcessOwner::stop(grace,kill)` — `terminate → grace → kill → killWait`, после `forgetServer()` | затваря връзките и **не пипа** чуждия процес; логва „the server on … is not ours — leaving it running" |
| Деструктор | спира своя процес с къси срокове | нищо |

Двете никога не са едновременно истина: `launch()` слага `m_owned = true`,
`adoptExistingServer()` — `m_reused = true`; `onProcessFinished()` и `fail()`
нулят `m_owned`.

Порядък в `start()`: валидация (синхронна, за да има `failed()` веднага) →
`sweepOrphans()` → `probeExistingServer()` → собствен процес **само** ако и
двете не намерят нищо. Повторен `start()` при `Starting`/`Ready` е no-op — втори
процес не се пуска.

## Готовност без блокиране

Нито един `waitForStarted()`/`waitForFinished()` в пътя на старта:

- `beginHealthPolling()` стартира `QTimer` (`healthIntervalMs`) и еднократен
  таймер за `readyTimeoutMs`;
- `pollHealth()` пуска `GET` през един `QNetworkAccessManager` и **не трупа
  заявки** — ако предишният poll още не е отговорил, този се пропуска;
- `onHealthReply()`: `200` → `Ready` + `started(port)`; `503` („loading") или
  липса на отговор → следващият poll;
- пробата „има ли вече сървър" е със `setTransferTimeout(kProbeTimeoutMs=800)`,
  тоест липсващ сървър не спира нишката за 120 s;
- блокиращи изчаквания има **само** в `stop()`/`fail()` — там ги изисква
  `Daqster::ProcessOwner` (REQ-SW-PL-050), със срокове от spec-а.

`stop()` преди `started(port)` е безопасен: `ProcessOwner` изчаква exec и **не
праща сигнал** към процес, който още не е exec-нал (инвариантът от
REQ-SW-PL-050; тест `TestLocalServerService::stopBeforeStartedIsNoOp`).

## Осиротял процес: какво е, как се разпознава, как се чисти

**Осиротял** = процес на нашия сървър, чийто родител (GUI/`NodeRunner`/тест) е
умрял. Той продължава да слуша на порта и да държи паметта си, но никой няма да
го спре — при следващ старт портът е зает и паметта се трупа.

`sweepOrphans()` събира кандидати от **два** източника и после проверява
собственика:

1. `/proc` — живи процеси, чийто `argv[0]`/`argv[1]` е нашият изпълним файл
   (`matchesBinary()`) **и** чийто `--port` е в нашия обхват;
2. файлът със собствеността — записаният PID, пак сверен по име на изпълнимия
   файл, защото PID-овете се преизползват;
3. `chainIsAliveOwner()` върви нагоре по `/proc/<pid>/stat` (`kOwnerDepth` = 4
   нива) и търси живия собственик: собствения PID, `daqster*` или
   `NodeRunner`. Стигне ли `ppid ≤ 1` (init/systemd) — собственикът е мъртъв;
4. мъртвите се спират учтиво: `SIGTERM → kSweepGraceMs=500 → SIGKILL`
   (`terminatePid()`) и `forgetServer(pid)`; логът казва „orphaned process (its
   parent died) — stopping it: pid N".

Живият собственик никога не се пипа — това пази пуснатия от човека GUI и
сървъра му. Собственият PID се проверява изрично: тестовият бинарен файл не
носи име `daqster`/`NodeRunner`, тоест без тази проверка тестът би изчистил
собствения си жив сървър.

Цялото почистване е под `#ifdef Q_OS_UNIX` — на Windows `/proc` го няма, тоест
стъпката е no-op (и това е записано, а не скрито зад „работи").

## Файлът със собствеността

`<GenericDataLocation>/Daqster/local-servers.json` (`ownershipFilePath()`),
един JSON за **всички** видове:

```json
{"servers":[{"kind":"sd-server","pid":12345,"binary":"/…/sd-server","port":18081}]}
```

Видът е **поле**, не име на файл — иначе всеки нов сървър иска нов файл и нов
четец. Защо е нужен:

- преизползване между сцени: вторият старт пита `/health` на порта, на който
  **ние** сме пуснали сървър последния път (`recordedPortFor()`) — иначе би
  гледал `portRangeStart` и би вдигнал втори процес, когато първият е тръгнал
  на 18081, защото 18080 е заето;
- втори източник за сираци, когато `/proc` не помогне.

Записът се прави на `QProcess::started` — щом ОС даде номер, и се трие **само
за нашия PID** (`forgetServer()`; викове от `stop()`, `onProcessFinished()`,
`fail()`, деструктора). Файлът се чисти, когато остане празен.

## Провали и повторни опити

- липсващ/неизпълним бинарник или невалиден обхват → `failed()` **синхронно**,
  с името на файла — без 120 s таймаут;
- процесът умря **преди** готовност → още един опит на следващия порт
  (най-честата причина е загубена надпревара за порта; таван
  `kMaxStartAttempts = 3`);
- умря **след** готовност → `Failed` без рестарт: сървърът е загубил
  състоянието си и решението „вдигни го пак" е на този, който знае какво има в
  паметта му;
- портът се избира с реален `listen()` и веднага `close()` (`pickFreePort()`);
  между проверката и bind-а има прозорец — затова „couldn't bind" води до
  следващия порт, а не до провал на услугата;
- stdout/stderr на процеса минава ред по ред към `logLine()`
  (`drainOutput()`), последните 4 реда влизат в съобщението за провал
  (`kStderrTailLines`).

## Какво класът **не** прави

- не знае за каквато и да е предметна област — нито за модели, нито за нодове;
- не говори протокола **след** готовност (`/v1/chat/completions`,
  `/sdapi/v1/txt2img`, `inference`, …) — това е на клиента на конкретната
  услуга;
- не решава дали сървърът трябва да работи — решението е на викащия (`stop()`
  е изричен; политиките „дръж топъл"/„освободи" са на затварянето на сцената);
- не зарежда и не изселва нищо — той само пуска и спира процес.

## Тестове

`tests/plugins/common/test_local_server_service.{h,cpp}` — 10 passed
(8 теста + `initTestCase`/`cleanupTestCase`) в `daqster_common_tests`, срещу
**истински** stub процес (`tests/plugins/common/local_server_stub.cpp`; Qt Core
+ Network, нула предметни редове, нула GPU): старт→готовност→собственост,
преизползване от втора инстанция (реалният брой процеси от `/proc` остава
**1**), `stop()` не гаси чужд сървър, липсващ/неизпълним бинарник, crash след
готовност, `stop()` преди `started`, идемпотентност + повторен старт,
`pickFreePort` подминава заетия порт.

Портовете са ефимерни (свободен базов порт + обхват от 20), не
18080–18180: тестовете не бива да пипат жив сървър на машината.
