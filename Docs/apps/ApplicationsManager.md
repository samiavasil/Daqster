# ApplicationsManager

**Class**: `ApplicationsManager`  
**Inherits**: `QProcessManager`  
**Pattern**: Singleton  
**Location**: `src/apps/Daqster/ApplicationsManager.{h,cpp}`

## Overview

`ApplicationsManager` е специализиран process manager за Daqster приложението. Наследява `QProcessManager` и добавя специфична функционалност за Daqster environment setup.

## Key Features

### 1. AppImage Environment Detection
Автоматично разпознава дали приложението работи в AppImage контейнер и конфигурира пътищата съответно.

### 2. Plugin Path Configuration
Настройва `QT_PLUGIN_PATH` да сочи към Daqster плъгините:
```cpp
QT_PLUGIN_PATH=<install_dir>/plugins/Daqster
```

### 3. XDG Directory Setup
Конфигурира XDG директории за кеш, конфигурация и данни:
- `XDG_CACHE_HOME`
- `XDG_CONFIG_HOME`
- `XDG_DATA_HOME`

### 4. Headless Mode
Поддържа headless mode за пускане на множество плъгини без GUI (полезно за тестване).

## API

### Singleton Access
```cpp
ApplicationsManager& manager = ApplicationsManager::Instance();
```

### Start Application
```cpp
void StartApplication(const QString& name, 
                     const QStringList& arguments, 
                     QProcess::OpenMode mode = QProcess::ReadWrite);
```

### Get Descriptor
```cpp
bool GetAppDescryptor(const AppHndl_t& handle, 
                     AppDescriptor_t& descriptor) const;
```

### Headless Mode
```cpp
void SetHeadlessMode(bool enabled);
```

## Signals

### ApplicationEvent
```cpp
void ApplicationEvent(const AppHndl_t& appHandle, 
                     const AppEvent_t& event);
```

События:
- `APP_NA` - Not available
- `APP_STARTED` - Приложението стартира
- `APP_STOPED` - Приложението спря

## Type Aliases (Backward Compatibility)

За съвместимост със стар код:
```cpp
typedef ProcessEvent_t AppEvent_t;
typedef ProcessDescriptor_t AppDescriptor_t;
typedef ProcessHandle_t AppHndl_t;
```

## Implementation Details

### Environment Setup Override
```cpp
void setupProcessEnvironment(QProcess* process, 
                            const QString& name,
                            const QStringList& arguments) override;
```

Извиква базовата имплементация и добавя Daqster-specific настройки:
1. Определя дали е AppImage
2. Намира install директорията
3. Задава `QT_PLUGIN_PATH`
4. Конфигурира XDG paths
5. (Опционално) Добавя headless флаг

## Usage Example

```cpp
// Get singleton instance
ApplicationsManager& manager = ApplicationsManager::Instance();

// Connect to events
connect(&manager, &ApplicationsManager::ApplicationEvent,
        [](const auto& handle, const auto& event) {
    if (event == ApplicationsManager::APP_STARTED) {
        qDebug() << "Application started:" << handle;
    }
});

// Start a plugin
QStringList args = {"--config", "debug.ini"};
manager.StartApplication("MyPlugin", args);

// Enable headless for testing
manager.SetHeadlessMode(true);
```

## See Also

- [QProcessManager](../framework/QProcessManager.md) - Base class
- [Plugin System](../framework/PluginSystem.md) - Plugin loading mechanism
- [Daqster Application](./Daqster.md) - Main application
