#include "FilePlaybackModel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using QtNodes::NodeDataType;

namespace {

SampleType sampleTypeFromName(const QString &name)
{
    if (name == QLatin1String("int8"))    return SampleType::INT8;
    if (name == QLatin1String("uint8"))   return SampleType::UINT8;
    if (name == QLatin1String("int16"))   return SampleType::INT16;
    if (name == QLatin1String("uint16"))  return SampleType::UINT16;
    if (name == QLatin1String("int24"))   return SampleType::INT24;
    if (name == QLatin1String("uint24"))  return SampleType::UINT24;
    if (name == QLatin1String("int32"))   return SampleType::INT32;
    if (name == QLatin1String("uint32"))  return SampleType::UINT32;
    if (name == QLatin1String("float32")) return SampleType::FLOAT32;
    if (name == QLatin1String("float64")) return SampleType::FLOAT64;
    return SampleType::INT16;
}

SampleEndian endianFromName(const QString &name)
{
    return name == QLatin1String("BigEndian")
        ? SampleEndian::BigEndian
        : SampleEndian::LittleEndian;
}

bool descriptorFromJson(const QJsonObject &obj, SampledStreamDescriptor &desc)
{
    desc.sampleRate = obj.value("sampleRate").toDouble(0.0);

    const QJsonArray channels = obj.value("channels").toArray();
    desc.channels.clear();
    for (const QJsonValue &v : channels) {
        const QJsonObject chObj = v.toObject();
        StreamChannelDescriptor ch;
        ch.name = chObj.value("name").toString();
        ch.sampleType = sampleTypeFromName(chObj.value("type").toString());
        desc.channels.append(ch);
    }

    desc.endianness = endianFromName(obj.value("endianness").toString());
    desc.unit = obj.value("unit").toString();
    desc.amplitudeScale = obj.value("amplitudeScale").toDouble(1.0);
    desc.amplitudeOffset = obj.value("amplitudeOffset").toDouble(0.0);
    desc.domain = obj.value("domain").toString();
    desc.deviceId = obj.value("deviceId").toString();
    desc.sourceName = obj.value("sourceName").toString();
    desc.firstSampleTimestamp = obj.value("firstSampleTimestamp").toVariant().toLongLong();
    desc.expectedBufferSeconds = obj.value("expectedBufferSeconds").toDouble(0.0);

    return desc.sampleRate > 0.0 && !desc.channels.isEmpty();
}

} // namespace

FilePlaybackModel::FilePlaybackModel()
{
    m_widget = new FilePlaybackWidget();

    connect(m_widget, &FilePlaybackWidget::playRequested,
            this, &FilePlaybackModel::onPlayRequested);
    connect(m_widget, &FilePlaybackWidget::stopRequested,
            this, &FilePlaybackModel::onStopRequested);
    connect(m_widget, &FilePlaybackWidget::pathChanged,
            this, &FilePlaybackModel::onPathChanged);

    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout,
            this, &FilePlaybackModel::onTimerTick);
}

FilePlaybackModel::~FilePlaybackModel()
{
    stopPlayback();
    m_widget = nullptr;
}

QJsonObject FilePlaybackModel::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();
    modelJson["filePath"] = m_widget->filePath();
    return modelJson;
}

void FilePlaybackModel::load(QJsonObject const &p)
{
    m_widget->setFilePath(p.value("filePath").toString());
}

unsigned int FilePlaybackModel::nPorts(QtNodes::PortType portType) const
{
    return portType == QtNodes::PortType::Out ? 1 : 0;
}

QtNodes::NodeDataType FilePlaybackModel::dataType(QtNodes::PortType portType,
                                                  QtNodes::PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return SampledData().type();
}

std::shared_ptr<QtNodes::NodeData> FilePlaybackModel::outData(QtNodes::PortIndex port)
{
    Q_UNUSED(port);
    return m_output;
}

void FilePlaybackModel::setInData(std::shared_ptr<QtNodes::NodeData> data,
                                  QtNodes::PortIndex port)
{
    Q_UNUSED(data);
    Q_UNUSED(port);
    Q_ASSERT(0);
}

QWidget *FilePlaybackModel::embeddedWidget()
{
    return m_widget;
}

// ── Connection-count gating (model of SystemMonitorModel) ───────────────────

void FilePlaybackModel::outputConnectionCreated(QtNodes::ConnectionId const &conId)
{
    Q_UNUSED(conId);
    ++m_connectionCount;
    if (m_userStarted && m_connectionCount > 0)
        startPlayback();
}

void FilePlaybackModel::outputConnectionDeleted(QtNodes::ConnectionId const &conId)
{
    Q_UNUSED(conId);
    if (m_connectionCount > 0)
        --m_connectionCount;
    if (m_connectionCount == 0)
        stopPlayback();
}

// ── Widget slots ────────────────────────────────────────────────────────────

void FilePlaybackModel::onPlayRequested()
{
    m_userStarted = true;
    if (m_connectionCount > 0)
        startPlayback();
    else
        m_widget->setStatus(tr("No output connection"));
}

void FilePlaybackModel::onStopRequested()
{
    m_userStarted = false;
    stopPlayback();
}

void FilePlaybackModel::onPathChanged(const QString &path)
{
    Q_UNUSED(path);
    // Path change while playing is not allowed — the model keeps the loaded
    // buffer it is emitting. The Play button is the only way to (re)load.
}

// ── Playback helpers ────────────────────────────────────────────────────────

bool FilePlaybackModel::loadFile()
{
    const QString path = m_widget->filePath();
    if (path.isEmpty()) {
        m_widget->setStatus(tr("No file path set"));
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_widget->setStatus(tr("Cannot open %1: %2").arg(path, file.errorString()));
        return false;
    }
    m_data = file.readAll();
    file.close();

    QFile sidecar(path + QStringLiteral(".json"));
    if (!sidecar.open(QIODevice::ReadOnly)) {
        m_widget->setStatus(tr("Cannot open sidecar %1: %2")
                                .arg(path + QStringLiteral(".json"),
                                     sidecar.errorString()));
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(sidecar.readAll());
    sidecar.close();
    if (!doc.isObject() || !descriptorFromJson(doc.object(), m_descriptor)) {
        m_widget->setStatus(tr("Invalid sidecar JSON in %1")
                                .arg(path + QStringLiteral(".json")));
        return false;
    }

    const int frameBytes = m_descriptor.bytesPerFrame();
    if (frameBytes <= 0 || m_data.isEmpty()) {
        m_widget->setStatus(tr("Empty or invalid data file %1").arg(path));
        return false;
    }

    // Chunk size in samples — clamp so a chunk is at least one frame and at
    // most the whole file. 4096 samples is a good default for high-rate IQ.
    const int totalFrames = m_data.size() / frameBytes;
    m_chunkSize = qBound(1, 4096, totalFrames);
    m_position = 0;
    m_loaded = true;
    return true;
}

void FilePlaybackModel::startPlayback()
{
    if (m_timer.isActive())
        return;

    if (!m_loaded && !loadFile())
        return;

    // QTimer interval in ms = chunkSamples / sampleRate * 1000. For sub-ms
    // intervals (high sample rates) clamp to 1 ms — the tempo is approximate
    // but bounded; the data is emitted in order regardless.
    const double intervalMs = (static_cast<double>(m_chunkSize)
                               / m_descriptor.sampleRate) * 1000.0;
    m_timer.start(qMax(1, static_cast<int>(intervalMs)));
    updateStatus();
}

void FilePlaybackModel::stopPlayback()
{
    if (m_timer.isActive())
        m_timer.stop();
    m_loaded = false;
    m_position = 0;
    m_widget->setStatus(tr("Idle"));
}

void FilePlaybackModel::onTimerTick()
{
    const int frameBytes = m_descriptor.bytesPerFrame();
    const int totalFrames = m_data.size() / frameBytes;
    if (m_position >= totalFrames) {
        stopPlayback();
        return;
    }

    const int framesThisTick = qMin(m_chunkSize, totalFrames - m_position);
    const int byteOffset = m_position * frameBytes;
    const int byteCount = framesThisTick * frameBytes;

    QByteArray chunk = m_data.mid(byteOffset, byteCount);
    m_output = std::make_shared<SampledData>(chunk, m_descriptor);
    m_position += framesThisTick;

    emit dataUpdated(0);
    updateStatus();
}

void FilePlaybackModel::updateStatus()
{
    const int frameBytes = m_descriptor.bytesPerFrame();
    const int totalFrames = frameBytes > 0 ? m_data.size() / frameBytes : 0;
    const double durationSec = m_descriptor.sampleRate > 0.0
        ? static_cast<double>(totalFrames) / m_descriptor.sampleRate
        : 0.0;
    const double positionSec = m_descriptor.sampleRate > 0.0
        ? static_cast<double>(m_position) / m_descriptor.sampleRate
        : 0.0;

    m_widget->setStatus(tr("%1s / %2s")
                            .arg(positionSec, 0, 'f', 1)
                            .arg(durationSec, 0, 'f', 1));
}
