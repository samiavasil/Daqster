Ще прегледам ключови файлове (README, `main.cpp`, мениджър на приложенията, плъгини и основната рамка), за да обобщя целта и архитектурата на проекта.

[8 tools called]

### Общо описание
- **Daqster** е десктоп приложение и рамка (framework) на Qt5 за зареждане и стартиране на плъгини. Има собствена библиотека `frame_work` за управление на плъгини, отделно приложение-лоукър `Daqster`, и примери/демо плъгини.

### Архитектура
- **Приложение (`src/apps/Daqster`)**
  - `main.cpp`: инициализира Qt, парсира аргументи, стартира плъгини по име от командния ред или показва `AppToolbar`, инициализира `QPluginManager`, прави пробно зареждане/освобождаване на всички намерени плъгини.
  - `ApplicationsManager`: управлява външни процеси чрез `QProcess` (стартира, следи завършване, изпраща "quit" за чисто спиране).

- **Основна библиотека (`src/frame_work`)**
  - Библиотека `frame_work` (споделена `.so`), композирана от:
    - `QPluginManager`, `QPluginLoaderExt`, `QPluginInterface`, `QBasePluginObject`, `PluginDescription`, `PluginFilter`, GUI за управление на плъгини (`QPluginManagerGui`, `QPluginListView`).
  - Отговаря за откриване (`SearchForPlugins`), описания, зареждане/инстанциране на плъгини и (по избор) показване на GUI за тях.

- **Плъгини (`src/plugins`)**
  - Поддиректории: `node_editor/`, `QtCoinTrader/`, `tests/` (примерни/тестови плъгини).
  - Всеки плъгин дефинира интерфейс клас, наследяващ `Daqster::QPluginInterface`, с Qt метаданни чрез `Q_PLUGIN_METADATA` и връщане на `QBasePluginObject` от `CreatePluginInternal`.
    - Пример: `NodeEditorInterface.h` регистрира IID `"Daqster.PlugIn.QPluginInterface"` и JSON файл за метаданни.
    - Пример: `QtCoinTraderInterface.h` използва `IID_DAQSTER_PLUGIN_INTERFACE` и свой JSON.

- **Външни библиотеки (`src/external_libs`)**
  - `nodeeditor` (подмодул) – визуален редактор на нодове (източник/примери).
  - `qtrest_lib` – малка REST библиотека (Qt-базирана) за HTTP/REST заявки.
  - Интегрират се в билд-а и/или се инсталират в `bin/extlibs`.

### Поток на работа
1. Стартиране на `Daqster`:
   - Създава се `QApplication`, настройва се приложението и парсера.
   - Инициализира се `QPluginManager` и се извиква `SearchForPlugins`.
   - Ако има позиционни аргументи, търси плъгин по име и го стартира:
     - Създава се `QBasePluginObject`, вика се `Initialize()`.
   - Ако няма аргументи, показва се `AppToolbar` (основен UI).
   - Стартира се слушател на конзолата (`QConsoleListener`) – командата "quit" затваря приложението.
2. Управление на външни приложения:
   - `ApplicationsManager::StartApplication` стартира процес (`QProcess`) с `ManagedInputChannel`.
   - При спиране `KillAll` изпраща "quit\r\n" към stdin и изчаква до 10s, после `kill()`.

### Билд и изпълнение
- Проектът е хибриден: има CMake (`src/frame_work/CMakeLists.txt`, `src/plugins/CMakeLists.txt`) и qmake `.pro` файлове за части от дървото.
- README описва стъпки за билд с qmake + cmake и изпълнение с:
  - `LD_LIBRARY_PATH=./libs:./extlibs/lib/ ./Daqster`
- Бинарни артефакти и плъгини се намират под `build/Desktop-Debug/bin/` или `bin/`.

### Ключови идеи/детайли
- **Плъгин модел**: Qt plugin system + собствени дескриптори/филтри. Плъгините се идентифицират по `PLUGIN_NAME`/`PLUGIN_HASH`. JSON метаданни се ползват от `Q_PLUGIN_METADATA`.
- **UI**: Qt Widgets; има диалози/списъци за управление на плъгини (`QPluginManagerGui`), и `AppToolbar` като стартов тулбар.
- **Инструменти**: Вграден Node Editor плъгин (визуално свързване на възли), REST демо плъгин (`QtCoinTrader`) с QML ресурси.
- **Тестови плъгини**: `plugins/tests` съдържа примерни/шаблонни плъгини за разработка и проверка на фреймуърка.

### Какво “горе-долу представлява”
- **Рамка за модулни Qt приложения**, където ядро (`frame_work`) открива/зарежда плъгини, а основното приложение `Daqster` действа като лекият хост/стартер. Примерни плъгини демонстрират UI функционалности (Node Editor) и REST/QML приложение (QtCoinTrader). Подходящо за изграждане и пускане на независими инструменти като плъгини под един хост.

- Ако искаш, мога да:
  - пусна проекта локално и да валидирам зареждането на плъгините;
  - добавя нов плъгин шаблон;
  - опростя билд процеса (еднакво CMake за всичко).



  ### Кратък отговор
За професионален „портфолио“ вид: уеднакви билда (CMake навсякъде), изчисти репото (без build/ артефакти), силно README с екранни снимки и „Quick Start“, CI билд за Linux/macOS/Windows, лиценз + ръководства (CONTRIBUTING, CODE_OF_CONDUCT), стил/линт (clang-format/clang-tidy), базови тестове, релийзи с артефакти и версия.

### Приоритетна чеклист (конкретно за този проект)
- Общ Build
  - Мигрирай към единен top-level CMake (Qt5/Qt6), премахни qmake `.pro` или ги обгърни през CMake.
  - Out-of-source build, `install()` таргети за `frame_work`, плъгини и `Daqster`.
  - Конфигурирай RPATH/`LD_LIBRARY_PATH` автоматично; добави CPack/AppImage/NSIS за инсталация.
- Репо хигиена
  - Добави `.gitignore`; премахни проследени `build/`, `bin/`, генерирани файлове и външни `build-*`.
  - Задръж външни зависимости като `git submodule` или `FetchContent`, но без техните билд артефакти.
- Документация
  - Обнови `README.md` (1–2 скрийншота, badge-ове за CI, Quick Start, Run, Структура, Плъгин как се пише, Лиценз).
  - Добави `docs/` (MkDocs/Doxygen) с API и „How to create a plugin“.
  - Премахни самоиронични бележки за „ugly build“; замени с ясни инструкции.
- CI/CD
  - GitHub Actions: матрица Qt (Linux/macOS/Windows), build + unit тестове, артефакти (бинарник + плъгини).
  - Release workflow (tag → build → качи артефакти, генерирай release notes).
- Код качество
  - `clang-format` + `clang-tidy` конфиг; `pre-commit` hooks.
  - Стандартизирай C++ стандарт (напр. C++17), включи компилаторни предупреждения `-Wall -Wextra -Wpedantic`.
  - Консистентни имена (STOPED → STOPPED и т.н.). Английски за публичните интерфейси/съобщения.
- Тестове
  - Добави unit тестове (QtTest/Catch2) за `QPluginManager`, `PluginFilter`, `QPluginLoaderExt`.
  - Интеграционен тест: намиране/зареждане на плъгин от `bin/plugins`.
- Примерни плъгини и UX
  - Премести `plugins/tests` към `plugins/examples`; добави „template“ плъгин за бърз старт.
  - Осъвремени UI: главен прозорец по подразбиране + меню „Plugins…“ (GUI мениджър винаги достъпен).
  - Добави икони/branding, тъмна/светла тема.
- Правни и общност
  - Лиценз (MIT/BSD), `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `SECURITY.md`.
  - `CHANGELOG.md` (Keep a Changelog), семантично версиониране.
- Статичен анализ и надеждност
  - Включи Address/UB Sanitizers в Debug CI джоб.
  - Покритие на тестове (gcov/llvm-cov) и badge.
- Малки, но важни корекции в кода
  - `ApplicationsManager::GetAppDescryptor`: проверката използва `nextHndl` вместо подадения `Hndl` — логическа грешка.
  - Thread-safety за сингълтъните (`QPluginManager`, `ApplicationsManager`) или изрично документирване на нишкова политика.
  - Консистентно логиране; премахване на закоментиран код; const-correctness; smart pointers където е уместно.

### Как бих подредил изпълнението (стъпки с голям ефект)
1) README + .gitignore + премахване на проследени build артефакти.  
2) Единен CMake (top-level), `install()` таргети, `bin/plugins` подредба.  
3) GitHub Actions (Linux първо), артефакти от CI.  
4) clang-format/clang-tidy + pre-commit.  
5) Базови тестове и AddressSanitizer в CI.  
6) Release workflow с тагове и описания + скрийншоти.

Искаш ли да започна с:
- Подобрено `README.md` (с Quick Start, Screenshots, Plugin Guide) и
- `.gitignore` + изчистване на `build/` от репото,
след което да добавя минимален GitHub Actions workflow?