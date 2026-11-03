# Нишки и живот на процесите

Родител: [Архитектура](./README.md)

Тази страница е **авторитетът** за това какъв код има право да създава нишка
или процес в дървото. Тя не описва „как е днес" — описва правилото, което се
налага от `tests/plugins/nodeditor_nodes/check_no_bare_threads.cmake`.

Примитивите живеят в `src/plugins/common/Threading/`:

| Файл | Какво държи |
|---|---|
| `ThreadOwner.h` | нишка + QObject worker върху нея (header-only шаблон) |
| `ProcessOwner.{h,cpp}` | един дъщерен процес (`terminate → гратис → kill`) |
| `CancelToken.h` | флаг, който задача в общ пул носи **по стойност** |
| `ThreadingLog.h` | категорията `daqster.threading` (една дефиниция) |

## Правило 1 — извън `Threading/` няма гол `QThread`/`QProcess`

Извън `src/plugins/common/Threading/` е забранено:

- `new QThread`, `QThread::create`, `class … : QThread` (и `: public QThread`);
- `new QProcess` и `QProcess` като член **по стойност**.

Ползвай `Daqster::ThreadOwner` / `Daqster::ProcessOwner`. Причината не е
естетика: ръчно написаният живот на нишка се разминава в едно от четирите
стъпала (create → moveToThread → finished→deleteLater → quit + wait), а
грешката се вижда като crash при затваряне на сцена, не при компилация.
Проверката е статична и **пада** при нов `new QThread`.

## Правило 2 — `stop()` е идемпотентен, с latch, с краен срок, с резултат

`stop()`:

1. вдига latch **преди** `quit()`/`terminate()` — работа, пристигнала по време
   на спирането, се отказва, вместо да се подреди на нишка, която умира;
2. има **краен срок** — нищо не чака завинаги;
3. връща статус: `NotRunning | Stopped | Killed | TimedOut` (за нишка:
   `NotRunning | Stopped | TimedOut`);
4. `TimedOut` **се логва** — „не спря, оставям го вързан" е решение, което
   трябва да се види, не да се подразбира.

Вторият `stop()` връща `NotRunning`. `alive()`/`invoke()` отказват работа още
щом latch-ът е вдигнат, тоест и в милисекундите преди реалното присъединяване.

`ThreadOwner::stop(deadlineMs, terminateMs, terminateOnTimeout)`:
`terminateOnTimeout=false` е за нишка, която **не бива** да се убива (PlutoSDR
вътре в `iio_buffer_refill()`); тогава `TimedOut` + `detach()`.

## Правило 3 — деструкторът вика `stop()`

Всеки собственик спира работата си в деструктора си. Никой не разчита на реда
на унищожаване на съседни обекти: `ThreadOwner`/`ProcessOwner` го правят сами
(`~ThreadOwner()` / `~ProcessOwner()`), а моделът/адаптерът вика `stop()` в
своя деструктор, за да мине същият път и когато потребителят не е натиснал
„Stop".

## Правило 4 — задача, която може да живее след собственика, не пипа `this`

Callback/задача, която може да се изпълни **след** смъртта на собственика:

- носи `Daqster::CancelToken` **по стойност** и проверява
  `Daqster::isCancelled(token)` като първи statement, **преди** което и да е
  поле на `this`; или
- носи `QPointer` и проверява `if (!ptr) return;`.

`ThreadOwner::invoke()` вече отказва нови задачи след latch-а; това покрива
„опашката", но не и задача, която вече се изпълнява. За нея е токенът.

## Правило 5 — чужд/преизползван ресурс не се убива

Процес, който собственикът **не** е пуснал (напр. топъл `whisper-server` на
друг), се маркира с `ProcessOwner::adopt(p, false)` или `setOwned(false)`.
`stop()` тогава само забравя връзката — никога `terminate()`/`kill()`.
Същото важи и за споделен сървър, намерен на well-known порт: той има свой
собственик.

## Правило 6 — ред на затваряне на сцена

Единственият правилен ред (`FrameworkCore::SceneClose`):

1. `IStoppable::stop()` на всеки нод — **източниците първи**, топологично по
   връзките;
2. `ISceneLifecycle::onSceneClosed()` на всеки `INodeProvider`;
3. `deleteNode()` / `clearScene()`.

Извън `SceneClose` не се вика `deleteNode()` направо: изтриването на възел
изтрива връзките му **преди** модела, тоест `outputConnectionDeleted()` на
производителя идва след неговия `stop()`. Затова всяко worker-bound извикване
проверява latch-а (Правило 2) — вж. `AudioSourceDataModel::onCaptureThread()`.

## Правило 7 — в тестове

`QThread`/`QProcess` в **тестове** са позволени, но само през примитивите, освен
ако тестът не проверява точно примитива (напр.
`tests/plugins/common/test_process_owner.cpp` създава чужд процес нарочно, за
да провери `adopt`/`setOwned(false)`). Статичната проверка сканира
`src/plugins/**` — тестовете не са в обхвата ѝ.

## Как да добавя нов worker

```cpp
// .h — worker-ът е parentless QObject, живее на своята нишка.
class MyPoller : public QObject { Q_OBJECT
public slots: void poll(); };

Daqster::ThreadOwner<MyPoller> m_poller;   // член на модела
```

```cpp
// .cpp — старт веднъж; всяко извикване минава през owner-а.
m_poller.start([] { return new MyPoller(); });
if (MyPoller *w = m_poller.alive())
    connect(w, &MyPoller::ready, this, &Model::onReady);
m_poller.invoke([](MyPoller *w) { w->poll(); });   // само ако е нужно
// stop(): m_poller.stop(3000); — latch, deadline, резултат.
```

## Как да добавя нов процес

```cpp
Daqster::ProcessOwner m_proc;                 // член на адаптера
m_proc.setOwned(true);                        // наш; чужд → false
m_proc.start(binary, args);
QProcess *p = m_proc.process();               // borrowed, за сигнали
connect(p, &QProcess::finished, this, &Adapter::onFinished);
// stop(): m_proc.stop(3000, 1000);
```

`ProcessOwner::process()` е **заем** — свързвай сигнали, чети изхода, не го
трий. Собственикът трие процеса, който е пуснал (`finished → deleteLater`).
