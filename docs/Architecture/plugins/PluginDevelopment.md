# Plugin Development

Родител: [Plugins Subsystem](./README.md) | [Architecture Overview](../README.md)

Стъпки за създаване на нов плъгин за Daqster.

## 1. Създай скеле

```
src/plugins/MyPlugin/
├── CMakeLists.txt
├── MyPlugin.cpp
├── MyPlugin.h
└── ui/ (ако има UI)
```

## 2. Имплементирай интерфейс

```cpp
class MyPlugin : public QObject, public QPluginInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.daqster.plugin")
    Q_INTERFACES(QPluginInterface)
public:
    QString name() const override { return "MyPlugin"; }
    QString version() const override { return "1.0.0"; }
    void initialize() override { /* init */ }
    void shutdown() override { /* cleanup */ }
};
```

## 3. CMake

```cmake
create_plugin(MyPlugin
    SOURCES
        MyPlugin.cpp
        MyPlugin.h
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        frame_work
)
```

Или за plugin с UI, използващ NodeEditorWidget:

```cmake
create_plugin(MyNodePlugin
    SOURCES
        MyNodePlugin.cpp
        MyNodePlugin.h
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        QtNodes
        node_editor_widget
        frame_work
)
```

## 4. Регистрирай и зареди

```cpp
QPluginManager mgr;
mgr.addSearchPath("plugins/Daqster");
mgr.loadAll();
```

## 5. UI интеграция (опционално)

Добави widget и го expose-ни чрез интерфейса.

## 6. Тестове

- Unit tests в `src/plugins/tests/`
- Интеграционни тестове в приложение

## 7. Debug

```bash
export QT_DEBUG_PLUGINS=1
./Daqster
```
