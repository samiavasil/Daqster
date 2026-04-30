[Български](./INDEX.md) | [English](./INDEX.en.md)

# Documentation Index

This is the main navigation page. Only top-level entry points live here.

## Main Entry Points

- [Project README](../README.en.md) - Project overview, build instructions, structure
- [Architecture](./Architecture/README.en.md) - architecture hub and subsystem navigation
- [Development Topics](./development/README.md) - development workflow and debugging topics
- [Operations Topics](./operations/README.md) - build, maintenance, and operational topics
- [Porting Topics](./porting/README.md) - migration and compatibility topics

## Diagrams

Visual overviews of architecture and processes (PlantUML sources and generated SVGs):

### Architecture

#### Architecture Diagram — overall system architecture
![Architecture](./diagrams/architecture.svg)
[Source: architecture.puml](./diagrams/architecture.puml)

#### Framework Components — framework layers and classes
![Framework Components](./diagrams/framework_components.svg)
[Source: framework_components.puml](./diagrams/framework_components.puml)

#### Applications Manager — application layer
![Apps Architecture](./diagrams/apps_architecture.svg)
[Source: apps_architecture.puml](./diagrams/apps_architecture.puml)

### Processes

#### Startup Sequence — Daqster initialization
![Startup Sequence](./diagrams/startup_sequence.svg)
[Source: startup_sequence.puml](./diagrams/startup_sequence.puml)

#### Plugin Lifecycle — plugin lifecycle events
![Plugin Lifecycle](./diagrams/plugin_lifecycle.svg)
[Source: plugin_lifecycle.puml](./diagrams/plugin_lifecycle.puml)

### Documentation

#### Documentation Hierarchy — Docs/ folder structure

```plantuml
@startuml
!theme plain
top to bottom direction
skinparam packageStyle rectangle
skinparam padding 4
skinparam fontSize 9
skinparam maxMessageSize 100

together {
  package "Level 1: Entry Points" {
    [INDEX]
    [Architecture]
    [Operations]
    [Porting]
    [Development]
  }

  package "Level 2: Architecture Hub" {
    [BuildSystem]
    [Apps]
    [Framework]
    [Plugins]
  }

  package "Level 2: Development" {
    [DeveloperGuide]
    [Debug]
  }
}

package "Level 3: Details" {
  [Daqster.md]
  [ApplicationsManager.md]
  [QProcessManager.md]
  [ShutdownHandler.md]
  [PluginDevelopment.md]
}

INDEX --> Architecture
INDEX --> Development
INDEX --> Operations
INDEX --> Porting

Architecture --> BuildSystem
Architecture --> Apps
Architecture --> Framework
Architecture --> Plugins

Development --> DeveloperGuide
Development --> Debug

Apps --> Daqster.md
Apps --> ApplicationsManager.md
Framework --> QProcessManager.md
Framework --> ShutdownHandler.md
Plugins --> PluginDevelopment.md

@enduml
```

[PlantUML source](./diagrams/documentation_hierarchy.puml)
