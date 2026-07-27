# Template Plugin Daqster

## Purpose

Minimal Daqster plugin template — use this as a starting point when creating a new plugin. It demonstrates the 3-layer pattern required by the Daqster plugin infrastructure.

## File Structure

```
template_plugin_daqster/
├── DaqsterTeplateInterface.{h,cpp}    # Layer 1: QPluginInterface — factory
├── DaqsterTeplateInterface.json       # Plugin metadata (IID)
├── TemplatePluginObject.{h,cpp}       # Layer 2: QBasePluginObject — lifecycle
├── template.qrc                       # Qt Resource file
├── template.png                       # Plugin icon
└── CMakeLists.txt                     # Build configuration
```

## How to Use

1. Copy this directory to a new name under `src/plugins/`:
   ```bash
   cp -r src/plugins/tests/template_plugin_daqster src/plugins/MyPlugin
   ```

2. Rename classes in all files (find-replace):
   - `DaqsterTemplateInterface` → `MyPluginInterface`
   - `TemplatePluginObject` → `MyPluginObject`
   - `plugin_template_test` → `MyPlugin` (CMake target name)

3. Update `CMakeLists.txt`:
   - Change `project()` name
   - Change `create_plugin()` target name
   - Add any additional `REQUIRES_LIBRARIES` (Qt modules, external libs, `frame_work`)

4. Add to root `CMakeLists.txt`:
   ```cmake
   add_subdirectory(src/plugins/MyPlugin)
   ```

5. Build:
   ```bash
   cmake --build build --target MyPlugin
   ```

## Three-Layer Pattern

| Layer | Class | Purpose |
|-------|-------|---------|
| **Interface** | `QPluginInterface` subclass | Factory — creates plugin object, provides metadata |
| **Object** | `QBasePluginObject` subclass | Lifecycle — `Initialize()`, `DeInitialize()` |
| **Node Provider** *(optional)* | `INodeProvider` subclass | Delivers node models to Node Editor IDE |

## CMakeLists.txt Template

```cmake
create_plugin(MyPlugin
    SOURCES
        MyPluginInterface.cpp
        MyPluginObject.cpp
        MyPluginInterface.h
        MyPluginObject.h
    REQUIRES_LIBRARIES
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        frame_work
)
```

**Note:** The output `.so` filename must contain "plugin" — `QPluginManager` filters by this substring. The `create_plugin()` macro handles this automatically via `OUTPUT_NAME`.
