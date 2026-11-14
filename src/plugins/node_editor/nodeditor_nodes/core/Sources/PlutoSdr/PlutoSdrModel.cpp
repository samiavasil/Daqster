#include "PlutoSdrModel.h"

#include "LogCategories.h"

#include <QJsonArray>
#include <QJsonObject>

using QtNodes::NodeDataType;

PlutoSdrModel::PlutoSdrModel()
{
    m_engine = new PlutoSdrEngine(this);

    // Engine → model (auto/queued: the engine emits from the worker thread).
    connect(m_engine, &PlutoSdrEngine::samplesReady,
            this, &PlutoSdrModel::onSamplesReady);
    connect(m_engine, &PlutoSdrEngine::statusChanged,
            this, &PlutoSdrModel::onStatusChanged);
    connect(m_engine, &PlutoSdrEngine::errorOccurred,
            this, &PlutoSdrModel::onErrorOccurred);

    updateEngineConfig();
}

PlutoSdrModel::~PlutoSdrModel()
{
    // Single shutdown path: stop() joins the stream thread before the engine
    // (child) is destroyed (REQ-SW-PL-050).
    stop();
}

void PlutoSdrModel::stop()
{
    // Idempotent: the engine's stop() is safe to call when not running.
    if (m_engine)
        m_engine->stop();
    m_userStarted = false;
}

QJsonObject PlutoSdrModel::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();

    // Config values are saved by the GUI widget via NodeWidgetFactory
    // Core model stores only the URI for the engine
    modelJson["uri"] = m_engine ? m_engine->uri() : QStringLiteral("ip:192.168.2.1");
    modelJson["frequencyMhz"] = m_engine ? m_engine->frequencyMhz() : 98.5;
    modelJson["sampleRateMsps"] = m_engine ? m_engine->sampleRateMsps() : 2.4;
    modelJson["gainMode"] = m_engine ? m_engine->gainMode() : QStringLiteral("manual");
    modelJson["gainDb"] = m_engine ? m_engine->gainDb() : 30.0;

    return modelJson;
}

void PlutoSdrModel::load(QJsonObject const &p)
{
    if (m_engine) {
        m_engine->setUri(p.value("uri").toString(QStringLiteral("ip:192.168.2.1")));
        m_engine->setFrequencyMhz(p.value("frequencyMhz").toDouble(98.5));
        m_engine->setSampleRateMsps(p.value("sampleRateMsps").toDouble(2.4));
        m_engine->setGainMode(p.value("gainMode").toString(QStringLiteral("manual")));
        m_engine->setGainDb(p.value("gainDb").toDouble(30.0));
    }
    updateEngineConfig();
}

unsigned int PlutoSdrModel::nPorts(QtNodes::PortType portType) const
{
    return portType == QtNodes::PortType::Out ? 1 : 0;
}

QtNodes::NodeDataType PlutoSdrModel::dataType(QtNodes::PortType portType,
                                              QtNodes::PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return SampledData().type();
}

std::shared_ptr<QtNodes::NodeData> PlutoSdrModel::outData(QtNodes::PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

void PlutoSdrModel::setInData(std::shared_ptr<QtNodes::NodeData> data,
                              QtNodes::PortIndex port)
{
    Q_UNUSED(data);
    Q_UNUSED(port);
    Q_ASSERT(0);
}

// ── Connection-count gating (model of VideoFileSourceNode) ──────────────────

void PlutoSdrModel::outputConnectionCreated(QtNodes::ConnectionId const &conId)
{
    Q_UNUSED(conId);
    ++m_connectionCount;
    setStreamingEnabled(m_userStarted && m_connectionCount > 0);
}

void PlutoSdrModel::outputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    Q_UNUSED(conId);
    if (m_connectionCount > 0)
        --m_connectionCount;
    setStreamingEnabled(m_userStarted && m_connectionCount > 0);
}

// ── Public slots called by GUI widget via NodeWidgetFactory ────────────────

void PlutoSdrModel::onStartRequested()
{
    m_userStarted = true;
    setStreamingEnabled(m_connectionCount > 0);
}

void PlutoSdrModel::start()
{
    m_userStarted = true;
    setStreamingEnabled(m_connectionCount > 0);
}

void PlutoSdrModel::onStopRequested()
{
    m_userStarted = false;
    m_engine->stop();
}

void PlutoSdrModel::onConfigChanged()
{
    updateEngineConfig();
}

// ── Engine slots ────────────────────────────────────────────────────────────

void PlutoSdrModel::onSamplesReady(const QByteArray &buffer, double sampleRateHz, int channels)
{
    Q_UNUSED(channels);

    // IQ samples ARE sampled data: 2 channels (I, Q), int16, interleaved —
    // exactly what SampledStreamDescriptor describes (REQ-SW-PL-040 §3).
    SampledStreamDescriptor desc;
    desc.sampleRate = sampleRateHz;
    desc.channels = {
        {QStringLiteral("I"), SampleType::INT16},
        {QStringLiteral("Q"), SampleType::INT16},
    };
    desc.endianness = SampleEndian::LittleEndian;
    desc.unit = QStringLiteral("raw");
    desc.domain = QStringLiteral("iq");
    desc.deviceId = QStringLiteral("plutosdr");
    desc.sourceName = QStringLiteral("PlutoSky 7020-SDR");

    m_output = std::make_shared<SampledData>(buffer, desc);

    // GUI widget (if present) will be updated via its own connection to engine
    emit dataUpdated(0);
}

void PlutoSdrModel::onStatusChanged(const QString &status)
{
    // GUI widget (if present) will be updated via its own connection to engine
    if (status.startsWith(QLatin1String("error:")))
        return;
    // Note: no m_widget->setStatus() - GUI widget connects to engine directly
    Q_UNUSED(status);
}

void PlutoSdrModel::onErrorOccurred(const QString &message)
{
    // GUI widget (if present) will be updated via its own connection to engine
    Q_UNUSED(message);
}

// ── Helpers ─────────────────────────────────────────────────────────────────

void PlutoSdrModel::updateEngineConfig()
{
    if (m_engine) {
        // Config values are applied by GUI widget via NodeWidgetFactory
        // Core model just ensures engine is running if needed
        if (m_userStarted && m_connectionCount > 0) {
            m_engine->start();
        }
    }
}

void PlutoSdrModel::setStreamingEnabled(bool enabled)
{
    if (enabled)
        m_engine->start();
    else
        m_engine->stop();
    // GUI widget (if present) will update its own running state
}

QJsonObject PlutoSdrModel::configSchema() const
{
    // Describes exactly the object save() emits (REQ-SW-PL-091).
    // "x-restart" is the per-field answer the control plane reads
    // when a config write lands on a running node.
    QJsonObject properties;

    // Every field here is an argument of the libiio context/stream setup, which
    // happens once in start(): the running capture never re-reads them.
    //
    // Config values are saved by the GUI widget; the core model stores them on
    // its engine, so this schema is the contract the control plane diffs.
    properties.insert(QStringLiteral("uri"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("string")},
        {QStringLiteral("default"), QStringLiteral("ip:192.168.2.1")},
        {QStringLiteral("x-restart"), true},
        {QStringLiteral("description"),
         QStringLiteral("libiio context URI of the PlutoSDR")}});

    properties.insert(QStringLiteral("frequencyMhz"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("number")},
        {QStringLiteral("minimum"), 70.0},
        {QStringLiteral("maximum"), 6000.0},
        {QStringLiteral("default"), 98.5},
        {QStringLiteral("x-restart"), true},
        {QStringLiteral("description"),
         QStringLiteral("RX centre frequency in MHz (the widget offers 70-6000)")}});

    properties.insert(QStringLiteral("sampleRateMsps"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("number")},
        {QStringLiteral("minimum"), 0.2},
        {QStringLiteral("maximum"), 7.5},
        {QStringLiteral("default"), 2.4},
        {QStringLiteral("x-restart"), true},
        {QStringLiteral("description"),
         QStringLiteral("RX sample rate in MSPS (the widget offers 0.2-7.5)")}});

    properties.insert(QStringLiteral("gainMode"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("string")},
        {QStringLiteral("enum"), QJsonArray{QStringLiteral("manual"),
                                            QStringLiteral("fast_attack"),
                                            QStringLiteral("slow_attack")}},
        {QStringLiteral("default"), QStringLiteral("manual")},
        {QStringLiteral("x-restart"), true},
        {QStringLiteral("description"), QStringLiteral("RX gain control mode")}});

    properties.insert(QStringLiteral("gainDb"), QJsonObject{
        {QStringLiteral("type"), QStringLiteral("number")},
        {QStringLiteral("minimum"), -10.0},
        {QStringLiteral("maximum"), 70.0},
        {QStringLiteral("default"), 30.0},
        {QStringLiteral("x-restart"), true},
        {QStringLiteral("description"),
         QStringLiteral("Manual RX gain in dB — only meaningful for "
                        "gainMode: \"manual\"")}});

    return QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), properties},
        {QStringLiteral("additionalProperties"), false}};
}
