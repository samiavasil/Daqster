# Developer Guide

This guide helps contributors and maintainers to get started with the
Daqster framework and develop plugins or new applications.

See also: [Framework API Reference](./FrameworkAPI.md) and [Architecture](./Architecture.md).

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

3. Run Daqster from build:

```bash
cd build/bin
./Daqster
```

## Code organization

- `src/frame_work` — framework core
- `src/apps/Daqster` — host application (GUI + toolbar)
- `src/plugins` — plugins and tests

## How to add a plugin

See `Docs/PluginDependencyManagement.md` for CMake registration. Basic steps:

1. Create plugin source under `src/plugins/YourPlugin`.
2. Use `register_plugin()` in your top-level CMake to declare dependencies.
3. Add `add_plugin_subdirectory()` so it gets built conditionally.

## How to extend `QProcessManager`

- Override `setupProcessEnvironment()` to customize environment variables.
- Override `onAllProcessesFinished()` to implement headless behavior.
- Connect to `ProcessEvent` to observe process lifecycle.

## Contributing

- Fork the repo and create a feature branch.
- Write tests where relevant and update documentation.
- Open PR against `refactoring` or `main` depending on scope.
