# Node Editor Library

Shared infrastructure for the Daqster Node Editor plugin system.

## Architecture

```
Library/
├── types/
│   ├── IStreamDecoder.h              # Interface: bytes → normalized doubles
│   ├── QDevIOStreamConfig.h          # Stream metadata (type, sampleRate, channels)
│   ├── StreamChannel.h               # StreamChannel + MixedStreamPayload
│   ├── GenericNumericTypes.h/.cpp    # SampleType, ChannelDescriptor, GenericNumericData
│   ├── NumericType.h                 # Template numeric data (int/double)
│   └── ChatGraphModel.h             # Loop-enabled DataFlowGraphModel
├── connectors/
│   ├── GenericQDevIoConnector.h/.cpp # Generic connector with metadata
│   └── NodeDataModelToQIODeviceConnector.h/.cpp  # Base connector class
├── display/
│   ├── QDevIoDisplayModel.h/.cpp     # Generic display with stacked widget
│   ├── QDevioDisplayModelUi.h/.cpp   # Chart UI widget
│   ├── XYSeriesIODevice.h/.cpp       # Ring buffer device for waveform display
│   ├── AudioCompat.h                 # Qt5/Qt6 audio compatibility
│   └── QtChartsCompat.h              # Qt5/Qt6 charts compatibility
├── threading/
│   └── EventThreadPull.h/.cpp        # Background worker thread management
└── decoders/
    └── AudioFrameDecoder.h/.cpp      # PCM sample decoder
```

## Key Types

### IStreamDecoder
Interface for decoding raw bytes into normalized doubles [-1, 1].

```cpp
class IStreamDecoder {
public:
    virtual int decode(const QByteArray& raw,
                       QVector<QVector<double>>& outChannels) = 0;
    virtual int channels() const = 0;
    virtual int bytesPerFrame() const = 0;
    virtual QString streamType() const = 0;  // "audio", "sensor", "sdr"
};
```

### QDevIOStreamConfig
Metadata about a stream's format, carried by connectors.

```cpp
struct QDevIOStreamConfig {
    QString type;              // "audio", "video", "sensor", "generic"
    int sampleRate = 0;        // Hz
    int bitsPerSample = 0;     // 8, 16, 24, 32
    int channels = 0;          // multiplexed channels
    double amplitudeScale = 1.0;
    QString unit = "V";
    QVector<QString> channelNames;
};
```

### GenericQDevIoConnector
Carries QIODevice + optional metadata. Display nodes read metadata to auto-route.

```cpp
class GenericQDevIoConnector : public NodeDataModelToQIODeviceConnector {
    void setIODevice(std::shared_ptr<QIODevice> device);
    void setStreamConfig(QDevIOStreamConfig config);
    void setPayload(MixedStreamPayload payload);
    // ...
};
```

### GenericNumericData
New port type `"generic_numeric"` for typed numeric data.

```cpp
enum class SampleType : uint8_t {
    INT16, UINT16, INT32, UINT32, FLOAT32, FLOAT64
};

class GenericNumericData : public NodeData {
    NodeDataType type() const override {
        return {"generic_numeric", "Generic"};
    }
};
```

## Display Routing

Display nodes use `QStackedWidget` to switch between views:

| Connector metadata | Display action | View |
|-------------------|---------------|------|
| `type="audio"` | Auto route | AudioWaveform + FFT |
| `type="video"` | Auto route | VideoImage (future) |
| `type="sensor"` | Auto route | SensorGauge or TimeChart |
| (no type) | **User config panel** | **Manual configuration** |
| `isMixed()=true` | Activate all matching views | Multiple widgets in stacked |

## Dependencies

- Qt Core, Multimedia, Charts
- QtNodes (external node editor framework)
- Daqster frame_work (plugin system)
