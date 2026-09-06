#include "JackDetectModel.h"

#include <QJsonObject>

JackDetectModel::JackDetectModel()
{
    m_engine = new JackDetectEngine(this);
    m_widget = new JackDetectWidget;

    connect(m_widget, &JackDetectWidget::startRequested,
            this, &JackDetectModel::onStartRequested);
    connect(m_widget, &JackDetectWidget::stopRequested,
            this, &JackDetectModel::onStopRequested);
    connect(m_widget, &JackDetectWidget::intervalChanged,
            this, &JackDetectModel::onIntervalChanged);

    connect(m_engine, &JackDetectEngine::jacksChanged,
            this, &JackDetectModel::onJacksChanged);
    connect(m_engine, &JackDetectEngine::statusChanged,
            this, &JackDetectModel::onStatusChanged);
}

JackDetectModel::~JackDetectModel()
{
    // Clean shutdown: stop the polling timer (REQ-SW-PL-046 AC 6).
    m_engine->stop();
    m_widget = nullptr; // owned by the node/view framework
}

QJsonObject JackDetectModel::save() const
{
    QJsonObject modelJson;
    modelJson["name"] = name();
    modelJson["intervalSeconds"] = m_widget->intervalSeconds();
    return modelJson;
}

void JackDetectModel::load(QJsonObject const &p)
{
    if (p.contains("intervalSeconds"))
        m_widget->setIntervalSeconds(p["intervalSeconds"].toDouble(0.5));
}

unsigned int JackDetectModel::nPorts(QtNodes::PortType portType) const
{
    return portType == QtNodes::PortType::Out ? 1u : 0u;
}

QtNodes::NodeDataType JackDetectModel::dataType(QtNodes::PortType portType,
                                                QtNodes::PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return SampledData().type();
}

std::shared_ptr<QtNodes::NodeData> JackDetectModel::outData(QtNodes::PortIndex port)
{
    Q_UNUSED(port);
    return m_lastData;
}

void JackDetectModel::setInData(std::shared_ptr<QtNodes::NodeData> data,
                                QtNodes::PortIndex port)
{
    Q_UNUSED(data);
    Q_UNUSED(port);
    Q_ASSERT(0);
}

QWidget *JackDetectModel::embeddedWidget()
{
    return m_widget;
}

void JackDetectModel::outputConnectionCreated(QtNodes::ConnectionId const &conId)
{
    Q_UNUSED(conId);
    ++m_connectionCount;
    setPollingEnabled(m_connectionCount > 0);
}

void JackDetectModel::outputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    Q_UNUSED(conId);
    if (m_connectionCount > 0)
        --m_connectionCount;
    setPollingEnabled(m_connectionCount > 0);
}

void JackDetectModel::onStartRequested()
{
    m_userStarted = true;
    setPollingEnabled(true);
}

void JackDetectModel::onStopRequested()
{
    m_userStarted = false;
    setPollingEnabled(false);
}

void JackDetectModel::onIntervalChanged(double seconds)
{
    m_engine->setPollIntervalMs(static_cast<int>(seconds * 1000.0));
}

void JackDetectModel::onJacksChanged(const QVector<JackDetectEngine::JackState> &jacks)
{
    m_lastData = buildSampledData(jacks);
    m_widget->setJacks(jacks);
    emit dataUpdated(0);
}

void JackDetectModel::onStatusChanged(const QString &status)
{
    m_widget->setStatusText(status);
}

void JackDetectModel::setPollingEnabled(bool enabled)
{
    const bool shouldRun = enabled && m_userStarted;
    if (shouldRun)
        m_engine->start();
    else
        m_engine->stop();
    m_widget->setRunning(shouldRun);
}

std::shared_ptr<SampledData> JackDetectModel::buildSampledData(
    const QVector<JackDetectEngine::JackState> &jacks) const
{
    SampledStreamDescriptor desc;
    desc.sampleRate = 1.0 / m_widget->intervalSeconds();
    for (const JackDetectEngine::JackState &jack : jacks)
        desc.channels.append({jack.name, SampleType::FLOAT32});
    desc.endianness = SampleEndian::LittleEndian;
    desc.unit = QStringLiteral("normalized");
    desc.domain = QStringLiteral("jack");
    desc.deviceId = QStringLiteral("hda");
    desc.sourceName = QStringLiteral("HDA Jack Detect");

    // One frame of N FLOAT32 samples (interleaved layout): 0.0 = unplugged,
    // 1.0 = plugged.
    QByteArray buffer;
    buffer.resize(desc.bytesPerFrame());
    float *ptr = reinterpret_cast<float *>(buffer.data());
    for (int i = 0; i < jacks.size(); ++i)
        ptr[i] = jacks.at(i).present ? 1.0F : 0.0F;

    return std::make_shared<SampledData>(buffer, desc);
}