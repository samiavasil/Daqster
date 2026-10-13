# Node Editor — ADR (Architecture Decision Records)

Родител: [Node Editor](../README.md) | [Architecture](../../README.md)

Записи на архитектурните решения за Node Editor подсистемата. ADR-ите са
**исторически** — описват решението в момента, в който е взето; пътищата/имената
вътре може да са остарели (всеки носи бележка, ако е преименуван).

| ADR | Решение | Статус |
|-----|---------|--------|
| [ADR-001](ADR-001-stream-routing.md) | **Stream routing** — metadata-driven рутиране; конекторът носи `QDevIOStreamConfig`, дисплеят авто-рутира по тип, fallback към ръчна конфигурация | Accepted |
| [ADR-002](ADR-002-generic-numeric-port.md) | **Generic numeric port** — нов port тип `generic_numeric` + `GenericNumericData` (типизирани сamples, multi-channel) | Accepted |
| [ADR-003](ADR-003-plugin-separation.md) | **Plugin separation (library vs demo)** — трислойна архитектура: shared support library / IDE shell / domain-specific nodes | Accepted |

Виж още: [Core/GUI Split](../../core-gui-split.md) · [Capabilities](../../capabilities.md).
