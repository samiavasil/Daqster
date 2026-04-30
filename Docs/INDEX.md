[Български](./INDEX.md) | [English](./INDEX.en.md)

# Индекс на документацията

Това е главната навигационна страница. Тук стоят само най-горните входни точки.

## Основни входни точки

- [Project README](../README.md) - общ преглед на проекта, инструкции за build и структура
- [Architecture](./Architecture/README.md) - архитектурен хъб и връзки към подсистемите
- [Development Topics](./development/README.md) - разработка, дебъг и workflow за contributors
- [Operations Topics](./operations/README.md) - build, поддръжка и operational теми
- [Porting Topics](./porting/README.md) - пренасяне и съвместимост между версии

## Диаграми

Визуални преглади на архитектурата и процесите (PlantUML източници и генерирани SVG):

### Архитектура

#### Architecture Diagram — обща система архитектура
![Architecture](./diagrams/architecture.svg)
[Източник: architecture.puml](./diagrams/architecture.puml)

#### Framework Components — framework слојеви и класи
![Framework Components](./diagrams/framework_components.svg)
[Източник: framework_components.puml](./diagrams/framework_components.puml)

#### Applications Manager — приложение слој
![Apps Architecture](./diagrams/apps_architecture.svg)
[Източник: apps_architecture.puml](./diagrams/apps_architecture.puml)

### Процеси

#### Startup Sequence — инициализация на Daqster
![Startup Sequence](./diagrams/startup_sequence.svg)
[Източник: startup_sequence.puml](./diagrams/startup_sequence.puml)

#### Plugin Lifecycle — жизнен цикъл на плъгини
![Plugin Lifecycle](./diagrams/plugin_lifecycle.svg)
[Източник: plugin_lifecycle.puml](./diagrams/plugin_lifecycle.puml)

### Документация

#### Documentation Hierarchy — структура на Docs/ папката

```plantuml
@startuml
!theme plain
top to bottom direction
skinparam packageStyle rectangle
skinparam padding 4
skinparam fontSize 9
skinparam maxMessageSize 100

together {
  package "Ниво 1: Входни точки" {
    [INDEX]
    [Architecture]
    [Operations]
    [Porting]
    [Development]
  }

  package "Ниво 2: Архитект. хъб" {
    [BuildSystem]
    [Apps]
    [Framework]
    [Plugins]
  }

  package "Ниво 2: Разработка" {
    [DeveloperGuide]
    [Debug]
  }
}

package "Ниво 3: Детайли" {
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

[PlantUML източник](./diagrams/documentation_hierarchy.puml)
