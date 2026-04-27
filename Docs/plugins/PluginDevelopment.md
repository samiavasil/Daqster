# Plugin Development

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
add_library(MyPlugin SHARED
    MyPlugin.cpp
    MyPlugin.h
)

target_link_libraries(MyPlugin PRIVATE Qt6::Core Qt6::Widgets)
set_target_properties(MyPlugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/Daqster
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
