# REQ-SW-016: Platform Shutdown Handler

- **Статус:** DONE
- **Приоритет:** P1
- **Отговорник (роля):** Implementation
- **Дата:** 2026-08-02
- **Родител:** —
- **Зависи от:** —

## Описание

Ретроспективно изискване за платформеното graceful-shutdown на Daqster:
абстрактен `ShutdownHandler` с фабрика, Unix реализация (self-pipe + сигнали)
и Windows реализация (`SetConsoleCtrlHandler`).

## Acceptance Criteria

- [x] 1. Фабрика `ShutdownHandler::create(parent)` връща правилния платформен
       handler: `WindowsShutdownHandler` при `Q_OS_WIN`, иначе
       `UnixShutdownHandler` (`ShutdownHandlerFactory.cpp`).
- [x] 2. Unix: `UnixShutdownHandler::initialize()` инсталира `std::signal(SIGINT,
       SIGTERM)` с async-signal-safe write към self-pipe и реагира през
       `QSocketNotifier` → emit `shutdownRequested()`.
- [x] 3. Windows: `WindowsShutdownHandler::initialize()` регистрира
       `SetConsoleCtrlHandler(consoleCtrlHandler, TRUE)`; handler-ът покрива
       `CTRL_C_EVENT`/`CTRL_BREAK_EVENT`/`CTRL_CLOSE_EVENT`/`CTRL_LOGOFF_EVENT`/
       `CTRL_SHUTDOWN_EVENT` и emit-ва `shutdownRequested()` (QueuedConnection).

## Проследимост

- **Коммити:** `0907d06` (feat: add ShutdownHandler::create() factory + cross-platform paths), `a1bce4b` (fix: remove dangling m_notifier initializer in WindowsShutdownHandler), `027dc0a` (refactor: remove dead StdinShutdownHandler)
- **Код:** `src/frame_work/base/src/platform/ShutdownHandler.h`, `ShutdownHandlerFactory.cpp`, `UnixShutdownHandler.{h,cpp}`, `WindowsShutdownHandler.{h,cpp}`
- **Тестове:** Qt5 + Qt6 builds
