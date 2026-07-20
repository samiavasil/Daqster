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
  ├── node_editor_ide/BuiltInNodes/Library/ (headers)
  ├── QtNodes
  └── frame_work

node_editor_ide
  ├── BuiltInNodes/Library/ (core infrastructure)
  ├── BuiltInNodes/Audio/ (domain-specific, stays here for now)
  ├── BuiltInNodes/LLama/ (domain-specific, stays here for now)
  ├── QtNodes
  └── frame_work
```

## References
- `demo_nodeditor_nodes/CMakeLists.txt` — demo plugin build
- `node_editor_ide/CMakeLists.txt` — core plugin build
- `AudioDisplayModel.h` — demo node using Library headers
- `GenericDisplayNode.h` — demo node using Library types
