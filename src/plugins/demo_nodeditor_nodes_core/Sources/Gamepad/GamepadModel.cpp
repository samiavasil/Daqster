#include "GamepadModel.h"

#include "LogCategories.h"

#include <QJsonObject>

using QtNodes::NodeDataType;

namespace {

// Build the canonical gamepad SampledStreamDescriptor (REQ-SW-PL-042 §2):
// 12 FLOAT32 channels (4 axes + 8 buttons), sampleRate = poll rate,
// domain = "gamepad".
SampledStreamDescriptor makeGamepadDescriptor(double sampleRate)
{
    SampledStreamDescriptor desc;
    desc.sampleRate = sampleRate;
    desc.channels = {
        {QStringLiteral("axis_x"), SampleType::FLOAT32},
        {QStringLiteral("axis_y"), SampleType::FLOAT32},
        {QStringLiteral("axis_z"), SampleType::FLOAT32},
        {QStringLiteral("axis_rz"), SampleType::FLOAT32},
        {QStringLiteral("button_a"), SampleType::FLOAT32},
        {QStringLiteral("button_b"), SampleType::FLOAT32},
        {QStringLiteral("button_x"), SampleType::FLOAT32},
        {QStringLiteral("button_y"), SampleType::FLOAT32},
        {QStringLiteral("button_lb"), SampleType::FLOAT32},
        {QStringLiteral("button_rb"), SampleType::FLOAT32},
        {QStringLiteral("button_back"), SampleType::FLOAT32},
        {QStringLiteral("button_start"), SampleType::FLOAT32},
    };
    desc.endianness = SampleEndian::LittleEndian;
    desc.unit = QStringLiteral("normalized");
    desc.domain = QStringLiteral("gamepad");
    desc.deviceId = QStringLiteral("gamepad");
    desc.sourceName = QStringLiteral("USB Gamepad");
    return desc;
}

} // namespace

GamepadModel::GamepadModel()
{
    m_engine = new GamepadEngine(this);

    // Engine → model (same thread — QTimer based).
    connect(m_engine, &GamepadEngine::stateReady,
            this, &GamepadModel::onStateReady);
    connect(m_engine, &GamepadEngine::statusChanged,
            this, &GamepadModel::onStatusChanged);
    connect(m_engine, &GamepadEngine::errorOccurred,
            this, &GamepadModel::onErrorOccurred);

    updateEngineConfig();
}

GamepadModel::~GamepadModel()
{
    // Single shutdown path: stop() stops the polling timer + closes the fd
    // before the engine (child) is destroyed (REQ-SW-PL-042 AC 6, PL-050).
    stop();
}

void GamepadModel::stop()
{
    // Idempotent: the engine's stop() is safe to call when not polling.
    if (m_engine)
        m_engine->stop();
    m_userStarted = false;
}

/// Start polling programmatically (runtime autoStart, REQ-SW-PL-048).
/// Delegates to the same logic as onStartRequested().
void GamepadModel::start()
{
    m_userStarted = true;
    setPollingEnabled(m_connectionCount > 0);
}

QJsonObject GamepadModel::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();

    modelJson["devicePath"] = m_devicePath;
    modelJson["pollRateHz"] = m_pollRateHz;

    return modelJson;
}

void GamepadModel::load(QJsonObject const &p)
{
    m_devicePath = p.value("devicePath").toString(QStringLiteral("/dev/input/js0"));
    m_pollRateHz = p.value("pollRateHz").toInt(60);

    updateEngineConfig();
}

unsigned int GamepadModel::nPorts(QtNodes::PortType portType) const
{
    return portType == QtNodes::PortType::Out ? 1 : 0;
}

QtNodes::NodeDataType GamepadModel::dataType(QtNodes::PortType portType,
                                             QtNodes::PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return SampledData().type();
}

std::shared_ptr<QtNodes::NodeData> GamepadModel::outData(QtNodes::PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

void GamepadModel::setInData(std::shared_ptr<QtNodes::NodeData> data,
                             QtNodes::PortIndex port)
{
    Q_UNUSED(data);
    Q_UNUSED(port);
    Q_ASSERT(0);
}

// ── Connection-count gating (model of SystemMonitorModel) ───────────────────

void GamepadModel::outputConnectionCreated(QtNodes::ConnectionId const &conId)
{
    Q_UNUSED(conId);
    ++m_connectionCount;
    setPollingEnabled(m_userStarted && m_connectionCount > 0);
}

void GamepadModel::outputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    Q_UNUSED(conId);
    if (m_connectionCount > 0)
        --m_connectionCount;
    setPollingEnabled(m_userStarted && m_connectionCount > 0);
}

// ── Widget slots ────────────────────────────────────────────────────────────

void GamepadModel::onStartRequested()
{
    m_userStarted = true;
    setPollingEnabled(m_connectionCount > 0);
}

void GamepadModel::onStopRequested()
{
    m_userStarted = false;
    m_engine->stop();
    Q_EMIT statusChanged(QStringLiteral("Idle"));
}

void GamepadModel::onDevicePathChanged(const QString &path)
{
    m_devicePath = path;
    updateEngineConfig();
}

void GamepadModel::onPollRateChanged(int hz)
{
    m_pollRateHz = hz;
    updateEngineConfig();
}

// ── Engine slots ────────────────────────────────────────────────────────────

void GamepadModel::onStateReady(const GamepadState &s)
{
    // Gamepad state IS sampled data: 12 FLOAT32 channels, one "frame" per poll
    // — exactly what SampledStreamDescriptor describes (REQ-SW-PL-042 §2).
    SampledStreamDescriptor desc = makeGamepadDescriptor(m_pollRateHz);

    // One interleaved frame of 12 FLOAT32 values.
    QByteArray buffer;
    buffer.resize(12 * static_cast<int>(sizeof(float)));
    float *ptr = reinterpret_cast<float *>(buffer.data());
    ptr[0] = s.axisX;
    ptr[1] = s.axisY;
    ptr[2] = s.axisZ;
    ptr[3] = s.axisRz;
    ptr[4] = s.buttonA;
    ptr[5] = s.buttonB;
    ptr[6] = s.buttonX;
    ptr[7] = s.buttonY;
    ptr[8] = s.buttonLB;
    ptr[9] = s.buttonRB;
    ptr[10] = s.buttonBack;
    ptr[11] = s.buttonStart;

    m_output = std::make_shared<SampledData>(buffer, desc);

    Q_EMIT axisValuesChanged(s.axisX, s.axisY, s.axisZ, s.axisRz);
    Q_EMIT buttonStatesChanged(s.buttonA, s.buttonB, s.buttonX, s.buttonY,
                               s.buttonLB, s.buttonRB, s.buttonBack, s.buttonStart);

    emit dataUpdated(0);
}

void GamepadModel::onStatusChanged(const QString &status)
{
    Q_EMIT statusChanged(status);
}

void GamepadModel::onErrorOccurred(const QString &message)
{
    Q_EMIT statusChanged(message);
}

// ── Helpers ─────────────────────────────────────────────────────────────────

void GamepadModel::updateEngineConfig()
{
    m_engine->setDevicePath(m_devicePath);
    m_engine->setPollRate(m_pollRateHz);
}

void GamepadModel::setPollingEnabled(bool enabled)
{
    if (enabled)
        m_engine->start();
    else
        m_engine->stop();
}