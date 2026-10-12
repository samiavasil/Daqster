# ADR-002: Generic Numeric Data Port Type

## Status
Accepted

## Context
The node editor needs a way to pass typed numeric data between nodes (sensor readings, API data, processed signals). The existing `QDevIO` type is tied to QIODevice and doesn't support typed samples.

## Decision
Introduce a new port type `"generic_numeric"` with `GenericNumericData`:

```cpp
enum class SampleType : uint8_t {
    INT16, UINT16, INT32, UINT32, FLOAT32, FLOAT64
};

struct ChannelDescriptor {
    QString name;
    SampleType sampleType;
};

struct GenericStreamConfig {
    int sampleRate;
    QVector<ChannelDescriptor> channels;
};

class GenericNumericData : public NodeData {
    NodeDataType type() const override {
        return {"generic_numeric", "Generic"};
    }
};
```

## Consequences

### Positive
- Clean separation from QDevIO (which is tied to QIODevice)
- Typed samples avoid unnecessary conversion
- Multi-channel support built-in
- Display nodes can inspect config for auto-configuration

### Negative
- New port type means existing nodes can't connect to generic_numeric ports
- Requires explicit MUX/DEMUX nodes to bridge between QDevIO and generic_numeric

## Alternatives Considered

### Use QVariantMap for metadata
- Too generic, no type safety
- Display nodes can't auto-configure

### Extend QDevIO with metadata
- Coupled to QIODevice concept
- Can't represent non-device data sources (APIs, files, etc.)

## References
- `GenericNumericTypes.h` — type definitions
- `GenericDisplayNode.h` — display for this port type
- `DemuxNode.h` — splits mixed QDevIO into typed outputs
