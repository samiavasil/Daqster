# ADR-003: Plugin Separation (Library vs Demo)

## Status
Accepted

## Context
The node editor needs shared infrastructure (display models, connectors, decoders) that multiple plugins can use. Domain-specific nodes (Audio, LLM, etc.) should be in separate plugins for maintainability.

## Decision
Three-layer architecture:

1. **Library/** (in `node_editor_ide`) — Shared, reusable components with no domain-specific dependencies
2. **node_editor_ide** — Clean IDE shell with generic display infrastructure
3. **demo_nodeditor_nodes** — Domain-specific nodes (Number, Audio, LLM, MUX/DEMUX)

## Consequences

### Positive
- Library/ can be extracted into a shared library in the future
- Domain-specific nodes are isolated and optional
- New node types added without modifying core infrastructure
- Clear dependency hierarchy

### Negative
- Demo plugin depends on Library/ headers (include path coupling)
- Moving domain-specific code requires updating include paths
- Two plugins to maintain instead of one

## Dependencies

```
demo_nodeditor_nodes
  ├── NodeEditorLibrary.so (shared)
  ├── QtNodes
  └── frame_work

node_editor_ide
  ├── NodeEditorLibrary.so (shared)
  ├── BuiltInNodes/Sources/NumberSource/
  ├── BuiltInNodes/Displays/NumberDisplay/
  ├── BuiltInNodes/Operators/Modulo/
  ├── BuiltInNodes/Operators/ArithmeticLogic/
  ├── QtNodes
  └── frame_work
```

## References
- `demo_nodeditor_nodes/CMakeLists.txt` — demo plugin build
- `node_editor_ide/CMakeLists.txt` — core plugin + shared library build
- `AudioDisplayModel.h` — demo node using Library headers
- `GenericDisplayNode.h` — demo node using Library types
