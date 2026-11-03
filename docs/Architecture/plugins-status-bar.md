# Status bar на плъгините (`IStatusBarProvider`)

Родител: [Architecture](README.md) | [Индекс](../index.md)
Виж още: [Capabilities](capabilities.md) · [Plugins](plugins/README.md) · [Node Editor IDE](../plugins/node_editor_ide/README.md)

REQ: `REQ-SW-PL-079`

## Проблемът

Плъгинът държи състояние, което **не принадлежи на нито един нод**: кеширани
тегла на модел, заета VRAM, топъл inference сървър, индексиране във фонов
режим. Нито един node widget не може да го покаже, защото то не е на нода —
то е на плъгина. Единственото, което потребителят виждаше, беше `nvidia-smi`:

```text
$ nvidia-smi        # „8 GB заети" — това топъл модел, готов за преизползване,
                    #  или изтекла памет, която никой не освобождава?
```

Отговорът има значение: едното прави следващото отваряне на сцена бързо,
другото е дефект. Затова плъгинът трябва да може да каже сам какво държи —
с една кратка дума в статус бара и, ако трябва, с действие в менюто.

## Решението

Capability интерфейс `IStatusBarProvider`
(`src/plugins/common/capabilities/IStatusBarProvider.h`): плъгинът дава текст
за статус бара, tooltip с подробностите и списък действия; **IDE ги рендира**.
Интерфейсът е генеричен — нищо в него не е за AI, модели или VRAM; плъгин,
който съобщава „3 сензора streaming" е също толкова валиден.

```cpp
class IStatusBarProvider {
public:
    virtual ~IStatusBarProvider() = default;

    virtual QString statusId() const = 0;        // "ai-studio.models" — уникален
    virtual QString statusText() const = 0;      // кратък ред за статус бара
    virtual QString statusTooltip() const = 0;   // подробности (може многоредов)
    virtual int statusOrder() const { return 100; }

    struct Action { QString id; QString text; QString tooltip; };
    virtual QList<Action> statusActions() const { return {}; }
    virtual void triggerStatusAction(const QString& id) = 0;   // no-op при непознат id
};
```

Като `IStoppable`/`IWidgetProvider` това е **plain (non-`QObject`) интерфейс без
`Q_DECLARE_INTERFACE` IID** и без `QWidget`: само QtCore типове. Затова
провайдър може да живее в widget-free core плъгин и да се компилира в headless
билд — widgets създава хостът.

## Откриване

Както при `INodeProvider`/`IWidgetProvider` — през plugin registry-то:

```cpp
const QList<Daqster::IStatusBarProvider*> providers = pm->statusBarProviders();
```

`QPluginManager::statusBarProviders()` делегира на
`PluginRegistry::statusBarProviders()`, който обхожда всички заредени
plugin интерфейси и прави `dynamic_cast<Daqster::IStatusBarProvider*>` върху
всеки plugin object (интерфейсът е non-QObject, значи `qt_metacast()` не го
вижда — същата причина като при `IWidgetProvider`).

Обектите се връщат **инициализирани** (`ensureInitialized()`): редът показва
живото състояние на онова, което плъгинът строи в `Initialize()` (model runtime,
registry), а без инициализация би показал половин обект. Точно затова това
откриване е работа на GUI host, а не на headless probe.

## Как IDE-то го показва

`node_editor_ide` (`NodeEditorIdeObject`) строи `StatusBarController`
(`node_editor_ide/StatusBarController.{h,cpp}`):

| Аспект | Поведение |
|---|---|
| Ред | по един `QLabel` на провайдър, `addPermanentWidget()` — временното съобщение на статус бара не ги скрива |
| Ред на подредба | по `statusOrder()` (по-малко = по-наляво), при равенство по `statusId()` — стабилно между обновяванията |
| Tooltip | `statusTooltip()`; показва се при hover **и** при клик върху реда |
| Клик върху реда | показва tooltip-а. **Не** изпълнява действие — действията освобождават ресурси (тегла, процеси), затова са в менюто, където изборът е съзнателен |
| Меню | `Tools → Plugins`; по едно `QAction` на `statusActions()`, разделени по провайдър; изборът вика `triggerStatusAction(id)` |
| Празно меню | подменюто се скрива, докато няма нито едно действие (без мъртви записи) |
| Празен `statusText()` | редът се скрива (това е нормално състояние „няма какво да кажа", не грешка) |

## Кога се обновява

1. **На всеки 2 s** — `QTimer` (`StatusBarController::startAutoRefresh()`),
   `statusText()`/`statusTooltip()` на всеки провайдър.
2. **След зареждане на сцена** (`loadSceneFromFile`) и **след затваряне**
   (`closeCurrentScene`) — изричен `refresh()`, за да не се чака до 2 s.
3. **При промяна на списъка плъгини** — `discoverAndRegisterExternalNodes()`
   вика `setProviders()`, така че плъгин, зареден по-късно, получава реда си.
4. **След изпълнено действие** — `triggerAction()` обновява веднага, защото
   действието обикновено променя точно това число.
5. **При смърт на plugin object-а** — редът се маркира като мъртъв
   (`QObject::destroyed`), следващият `refresh()` го маха и хостовете
   преоткриват провайдърите.

## Какво **не** бива

- **Тежко изчисление в `statusText()`.** Вика се на всеки 2 s, 30 пъти в
  минута, докато прозорецът е отворен. Показвай вече известно число (брояч,
  кеширан размер). Никакво inference, `nvidia-smi`, обхождане на процеси или
  файлова система, за да отговориш.
- **Блокиране на GUI нишката.** Всички методи се викат от нишката на
  widget-ите. Ако числото се смята другаде — пази го готово и го връщай.
- **Смяна на `statusId()` по време на сесията.** Това е ключът, по който редът
  се проследява; смяната му прави втори ред.
- **Два провайдъра с един и същ `statusId()`.** Първият печели, вторият се
  игнорира с предупреждение в лога (по-добре един ред, отколкото два еднакви).
- **Хвърляне на изключение.** `statusText()`/`triggerStatusAction()` се викат
  зад `try/catch` — изключението се логва, редът се скрива, IDE-то живее. Това
  е предпазна мрежа, не разрешение.
- **Изпълнение на действие от `statusText()`.** Текстът се чете, нищо повече.

## Как се добавя провайдър

```cpp
class MyPluginObject : public Daqster::QBasePluginObject,
                       public Daqster::IStatusBarProvider
{
public:
    QString statusId() const override { return QStringLiteral("my-plugin.state"); }
    QString statusText() const override
    {
        return m_warmCount == 0 ? QString()
                                : tr("%1 warm · %2 GB").arg(m_warmCount).arg(m_warmGb, 0, 'f', 1);
    }
    QString statusTooltip() const override { return m_breakdown; }  // кеширан низ
    int statusOrder() const override { return 50; }

    QList<Action> statusActions() const override
    {
        return {{QStringLiteral("free"), tr("Free warm resources"), tr("Unload everything now")}};
    }
    void triggerStatusAction(const QString& id) override
    {
        if (id == QLatin1String("free"))
            freeEverything();          // непознат id = no-op
    }
};
```

Нищо не се регистрира ръчно: щом plugin object-ът имплементира интерфейса,
`PluginRegistry::statusBarProviders()` го намира.

## Тестове

- `tests/framework/registry/` → `status_bar_discovery_tests`:
  `statusProviderIsDiscovered`, `pluginWithoutTheCapabilityIsNotListed`,
  `disabledPluginIsNotListed` — фалшив plugin интерфейс, без `.so`.
- `tests/plugins/node_editor_ide/test_status_bar.cpp` (offscreen):
  `statusBarShowsProviderText`, `statusActionIsTriggered`, `emptyTextHidesLabel`,
  `missingProviderDoesNotCrashIde`, `providersAreOrderedByStatusOrder`,
  `duplicateStatusIdKeepsFirstRow`, `menuIsHiddenWithoutActions`.

## Какво остава за следващата стъпка

Публичната част е само capability-то и рендирането. Частният AI плъгин
(`AiStudioPluginObject`) го имплементира, за да показва топлите модели и
заетата VRAM — това е отделна стъпка в частното repo (`REQ-AI-*`), не тук.
