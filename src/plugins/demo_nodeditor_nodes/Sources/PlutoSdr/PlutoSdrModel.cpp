#include "PlutoSdrModel.h"

#include "LogCategories.h"

#include <QJsonObject>

using QtNodes::NodeDataType;

PlutoSdrModel::PlutoSdrModel()
{
    m_engine = new PlutoSdrEngine(this);
    m_widget = new PlutoSdrWidget();

    // Widget → model (GUI thread).
    connect(m_widget, &PlutoSdrWidget::startRequested,
            this, &PlutoSdrModel::onStartRequested);
    connect(m_widget, &PlutoSdrWidget::stopRequested,
            this, &PlutoSdrModel::onStopRequested);
    connect(m_widget, &PlutoSdrWidget::configChanged,
            this, &PlutoSdrModel::onConfigChanged);

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
    // Stop the stream thread cleanly before the engine (child) is destroyed.
    if (m_engine)
        m_engine->stop();

    // Widget lifetime is owned by the node/view framework.
    m_widget = nullptr;
}

QJsonObject PlutoSdrModel::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();

    modelJson["uri"] = m_widget->uri();
    modelJson["frequencyMhz"] = m_widget->frequencyMhz();
    modelJson["sampleRateMsps"] = m_widget->sampleRateMsps();
    modelJson["gainMode"] = m_widget->gainMode();
    modelJson["gainDb"] = m_widget->gainDb();

    return modelJson;
}

void PlutoSdrModel::load(QJsonObject const &p)
{
    m_widget->setUri(p.value("uri").toString(QStringLiteral("ip:192.168.2.1")));
    m_widget->setFrequencyMhz(p.value("frequencyMhz").toDouble(98.5));
    m_widget->setSampleRateMsps(p.value("sampleRateMsps").toDouble(2.4));
    m_widget->setGainMode(p.value("gainMode").toString(QStringLiteral("manual")));
    m_widget->setGainDb(p.value("gainDb").toDouble(30.0));

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

QWidget *PlutoSdrModel::embeddedWidget()
{
    return m_widget;
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

// ── Widget slots ────────────────────────────────────────────────────────────

void PlutoSdrModel::onStartRequested()
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

    emit dataUpdated(0);
}

void PlutoSdrModel::onStatusChanged(const QString &status)
{
    // Error statuses carry the detailed message via errorOccurred.
    if (status.startsWith(QLatin1String("error:")))
        return;
    m_widget->setStatus(status);
}

void PlutoSdrModel::onErrorOccurred(const QString &message)
{
    m_widget->setStatus(message);
}

// ── Helpers ─────────────────────────────────────────────────────────────────

void PlutoSdrModel::updateEngineConfig()
{
    m_engine->setUri(m_widget->uri());
    m_engine->setFrequencyMhz(m_widget->frequencyMhz());
    m_engine->setSampleRateMsps(m_widget->sampleRateMsps());
    m_engine->setGainMode(m_widget->gainMode());
    m_engine->setGainDb(m_widget->gainDb());
}

void PlutoSdrModel::setStreamingEnabled(bool enabled)
{
    if (enabled)
        m_engine->start();
    else
        m_engine->stop();
}