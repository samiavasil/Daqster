# ADR-001: Stream Routing Architecture

## Status
Accepted

## Context
The node editor needs to handle multiple stream types (audio, video, sensor, generic numeric) with a unified display system. Streams can be single-type or mixed (multiple types in one QDevIO stream).

## Decision
Use a metadata-driven routing system:

1. **Connector carries metadata** — `GenericQDevIoConnector` includes `QDevIOStreamConfig` with stream type, sample rate, channels, etc.
2. **Display auto-routes by type** — `QDevIoDisplayModel` uses `QStackedWidget` and switches views based on `config.type`
3. **Fallback to manual config** — When no metadata is present, show a config panel for user configuration
4. **Mixed streams activate all matching views** — Multiple views can be active simultaneously

## Consequences

### Positive
- Single display node handles all stream types
- New stream types added by just registering a new view
- Backward compatible with existing AudioNodeQdevIoConnector
- User can always override auto-routing

### Negative
- Slightly more complex than separate display nodes per type
- Requires connector authors to set metadata correctly

## Alternatives Considered

### Separate display nodes per type
- Simpler but leads to node explosion (AudioDisplay, SensorDisplay, VideoDisplay, etc.)
- No support for mixed streams without duplicate code

### Generic config panel only
- Maximum flexibility but no auto-routing
- User must configure every connection manually

## References
- `QDevIoDisplayModel.h` — stacked widget implementation
- `GenericQDevIoConnector.h` — connector with metadata
- `QDevIOStreamConfig.h` — stream metadata struct
