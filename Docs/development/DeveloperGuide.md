# Developer Guide

Parent: [Documentation Index](../INDEX.md)

This guide helps contributors and maintainers to get started with the
Daqster framework and develop plugins or new applications.

See also: [Framework](../Architecture/framework/README.md) and [Architecture](../Architecture/README.md).

## Scope

This document covers local development workflow, code layout, plugin integration,
and practical debugging entry points. For architectural structure use
[Architecture](../Architecture/README.md). For the build model and component templates use
[BuildSystemArchitecture.md](../Architecture/BuildSystemArchitecture.md).

## Getting started

1. Clone repository and init submodules:

```bash
git clone https://github.com/samiavasil/Daqster.git
git submodule update --init --recursive
```

2. Configure and build (Debug recommended):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

If you work with a specific Qt installation, pass `CMAKE_PREFIX_PATH` explicitly.

```bash
cmake -S . -B build \
	-DCMAKE_BUILD_TYPE=Debug \
	-DCMAKE_PREFIX_PATH=/path/to/Qt/5.15.2/gcc_64
cmake --build build -j
```

3. Run Daqster from build:

```bash
cd build/bin
./Daqster
```

## Code organization

- `src/frame_work` — framework core, process management, plugin loading, shutdown handling
- `src/apps/Daqster` — host application, toolbar, process orchestration
- `src/plugins` — runtime plugins and plugin test targets
- `src/external_libs` — bundled third-party libraries used by some plugins
- `cmake` — build helpers, component templates, dependency checks

## How to add a plugin

See [BuildSystemArchitecture.md](../Architecture/BuildSystemArchitecture.md) for the current build model. Basic steps:

1. Create plugin source under `src/plugins/YourPlugin`.
2. Use `create_plugin()` in the plugin's own `CMakeLists.txt` and declare dependencies in `REQUIRES_LIBRARIES`.
3. Add `add_subdirectory(src/plugins/YourPlugin)` in the root `CMakeLists.txt`.
4. Build and verify that the plugin is either enabled or cleanly skipped by the dependency checks.

Example:

```cmake
create_plugin(MyPlugin
		SOURCES
				MyPluginInterface.cpp
				MyPluginObject.cpp
		REQUIRES_LIBRARIES
				Qt${QT_VERSION_MAJOR}::Core
				Qt${QT_VERSION_MAJOR}::Gui
				frame_work
)
```

Typical verification flow:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target MyPlugin -j
```

## How to add an application

1. Create a new directory under `src/apps/YourApp`.
2. Use `create_application()` in the application's `CMakeLists.txt`.
3. Add `add_subdirectory(src/apps/YourApp)` in the root `CMakeLists.txt`.
4. Link against `frame_work` and the required Qt modules explicitly.

## How to extend `QProcessManager`

- Override `setupProcessEnvironment()` to customize environment variables.
- Override `onAllProcessesFinished()` to implement headless behavior.
- Connect to `ProcessEvent` to observe process lifecycle.

The main project example is `ApplicationsManager` under `src/apps/Daqster`.

## Contributing

- Fork the repo and create a feature branch.
- Write tests where relevant and update documentation.
- Open the PR against the current integration branch used by the project.

## Debugging

- Use [HowToDebugAppImage.md](./HowToDebugAppImage.md) for AppImage-specific debugging.
- Use `QT_DEBUG_PLUGINS=1` when diagnosing plugin loading issues.
- Configure with `-DDAQSTER_VERBOSE_DEPENDENCIES=ON` when debugging build-time dependency decisions.

Example:

```bash
cmake -S . -B build_check \
	-DUSE_QT6=OFF \
	-DDAQSTER_VERBOSE_DEPENDENCIES=ON \
	-DCMAKE_PREFIX_PATH=/path/to/Qt/5.15.2/gcc_64
```

## Diagrams / Hooks

We provide a small `pre-commit` hook that can render PlantUML diagrams before commit and
stage generated SVG/PNG files automatically. This is optional but recommended for a smoother
developer experience.

Installation:

```bash
# From repo root (one-time per clone):
./scripts/install-hooks.sh
```

The hook will attempt to locate PlantUML in this order:

- `plantuml` CLI on PATH
- `./tools/plantuml.jar` (recommended if you want repo-provided jar)
- `./plantuml.jar` (local copy)
- Docker (image `plantuml/plantuml:jetty`) if Docker is available

If PlantUML is not found, the hook runs in friendly mode and will allow the commit to proceed
but will print instructions. To enforce rendering (fail when PlantUML is absent), set
the environment variable `PLANTUML_HOOK_STRICT=1`.

To skip the hook for a single commit or push:

```bash
git commit -m "..." --no-verify
git push --no-verify
```

We also run PlantUML rendering in CI (GitHub Actions) to ensure canonical diagrams are
generated even if a developer does not run the hook locally.
