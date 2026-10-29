#include "AudioOutputNode.h"

#include <QJsonObject>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::PortIndex;
using QtNodes::PortType;

namespace {
/// Mirrors AudioPlaybackEngine's clamp — load() must not persist a buffer size
/// the engine would silently change (round-trip, AC3).
constexpr int kMinBufferMs = 20;
constexpr int kMaxBufferMs = 2000;
} // namespace

AudioOutputNode::AudioOutputNode()
{
    // The engine owns the device and the status text; the node only forwards it
    // to the widget, which re-renders on change.
    connect(&m_engine, &AudioPlaybackEngine::statusChanged,
            this, &AudioOutputNode::statusChanged);

    m_engine.setConfig(m_config.deviceId, m_config.volume,
                       m_config.bufferMs, m_config.enabled);
}

AudioOutputNode::~AudioOutputNode()
{
    // Single shutdown path: stop() closes the sink (REQ-SW-PL-050).
    stop();
}

void AudioOutputNode::start()
{
    m_engine.start();
}

void AudioOutputNode::stop()
{
    // Idempotent: stopping an already-stopped engine is a no-op.
    m_engine.stop();
}

QString AudioOutputNode::statusText() const
{
    return m_engine.statusText();
}

void AudioOutputNode::setConfig(const Config &config)
{
    m_config = config;
    m_config.volume = qBound(0.0, m_config.volume, 1.0);
    m_config.bufferMs = qBound(kMinBufferMs, m_config.bufferMs, kMaxBufferMs);

    // The engine decides what the change costs: volume is live, a device or
    // buffer-size change re-creates the sink and reopens it at once.
    m_engine.setConfig(m_config.deviceId, m_config.volume,
                       m_config.bufferMs, m_config.enabled);
}

QJsonObject AudioOutputNode::save() const
{
    // Base save() adds "model-name" = name() (REQ-SW-PL-057) — the
    // registration key in a .flow file.
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();

    modelJson[QStringLiteral("deviceId")] = m_config.deviceId;
    modelJson[QStringLiteral("volume")] = m_config.volume;
    modelJson[QStringLiteral("bufferMs")] = m_config.bufferMs;
    modelJson[QStringLiteral("enabled")] = m_config.enabled;
    return modelJson;
}

void AudioOutputNode::load(QJsonObject const &p)
{
    Config config;
    config.deviceId = p.value(QStringLiteral("deviceId")).toString(config.deviceId);
    config.volume = p.value(QStringLiteral("volume")).toDouble(config.volume);
    config.bufferMs = p.value(QStringLiteral("bufferMs")).toInt(config.bufferMs);
    config.enabled = p.value(QStringLiteral("enabled")).toBool(config.enabled);

    // A hand-edited file (or an older one) must not smuggle an out-of-range
    // volume/buffer size past the engine's clamp and into save().
    setConfig(config);

    // Restored values are what the widget must show; the sink itself is NOT
    // opened here — start() decides that (a loaded graph is not a running one).
    Q_EMIT configChanged();
}

QJsonObject AudioOutputNode::configSchema() const
{
    // Describes exactly the object save() emits (AC3). "model-name" is part of
    // that object (written by the base save(), REQ-SW-PL-057) and is read-only.
    QJsonObject properties;

    properties.insert(QStringLiteral("deviceId"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("string")},
        {QStringLiteral("default"), QString()},
        {QStringLiteral("description"),
         QStringLiteral("Stable audio output device id "
                        "(AudioCompat::deviceId()); empty = system default")}});

    properties.insert(QStringLiteral("volume"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("number")},
        {QStringLiteral("minimum"), 0.0},
        {QStringLiteral("maximum"), 1.0},
        {QStringLiteral("default"), 1.0},
        {QStringLiteral("description"), QStringLiteral("Linear volume, 0..1")}});

    properties.insert(QStringLiteral("bufferMs"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("integer")},
        {QStringLiteral("minimum"), kMinBufferMs},
        {QStringLiteral("maximum"), kMaxBufferMs},
        {QStringLiteral("default"), 200},
        {QStringLiteral("description"),
         QStringLiteral("Sink buffer in milliseconds — also the queue bound "
                        "for dropped blocks")}});

    properties.insert(QStringLiteral("enabled"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("boolean")},
        {QStringLiteral("default"), true},
        {QStringLiteral("description"),
         QStringLiteral("false = no sink is opened and incoming blocks are skipped")}});

    properties.insert(QStringLiteral("model-name"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("string")},
        {QStringLiteral("const"), QStringLiteral("AudioOutput")},
        {QStringLiteral("readOnly"), true},
        {QStringLiteral("description"), QStringLiteral("Node type (REQ-SW-PL-057)")}});

    return QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), properties}};
}

unsigned int AudioOutputNode::nPorts(PortType portType) const
{
    // Sink: one input ("sample"), no output.
    return portType == PortType::In ? 1u : 0u;
}

NodeDataType AudioOutputNode::dataType(PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    // Derived, never hardcoded: the same type the audio sources emit.
    return SampledData().type();
}

std::shared_ptr<NodeData> AudioOutputNode::outData(PortIndex port)
{
    Q_UNUSED(port);
    return nullptr;
}

void AudioOutputNode::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    if (portIndex != 0)
        return;

    const auto sampled = std::dynamic_pointer_cast<SampledData>(data);
    if (!sampled)
        return;   // nullptr / foreign data: nothing to play. The sink survives —
                  // tearing it down on a transient gap is exactly the click the
                  // persistent sink is meant to avoid.

    m_engine.push(*sampled);
}
