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
    m_timer = new QTimer(this);
    m_timer->setSingleShot(false);
    connect(m_timer, &QTimer::timeout,
            this, &FilePlaybackModel::onTimerTick);
}

FilePlaybackModel::~FilePlaybackModel()
{
    // Single shutdown path: stop() stops the playback timer (REQ-SW-PL-050).
    stop();
}

void FilePlaybackModel::stop()
{
    // Idempotent: stopPlayback() on an already-stopped timer is a no-op.
    stopPlayback();
    m_userStarted = false;
}

/// Start playback programmatically (runtime autoStart, REQ-SW-PL-048).
/// Delegates to the same logic as onPlayRequested().
void FilePlaybackModel::start()
{
    m_userStarted = true;
    if (m_connectionCount > 0)
        startPlayback();
}

QJsonObject FilePlaybackModel::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();
    modelJson["filePath"] = m_filePath;
    return modelJson;
}

void FilePlaybackModel::load(QJsonObject const &p)
{
    m_filePath = p.value("filePath").toString();
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

// ── Public slots (called by GUI widget via NodeWidgetFactory) ──────────────

void FilePlaybackModel::onPlayRequested()
{
    m_userStarted = true;
    if (m_connectionCount > 0)
        startPlayback();
    else
        emit statusChanged(tr("No output connection"));
}

void FilePlaybackModel::onStopRequested()
{
    m_userStarted = false;
    stopPlayback();
}

void FilePlaybackModel::onPathChanged(const QString &path)
{
    if (!m_playing) {
        m_filePath = path;
    }
}

// ── Playback helpers ───────────────────────────────────────────────────────

bool FilePlaybackModel::loadFile()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit statusChanged(tr("Cannot open %1: %2").arg(m_filePath, file.errorString()));
        return false;
    }

    QFile sidecar(m_filePath + ".json");
    if (!sidecar.open(QIODevice::ReadOnly)) {
        emit statusChanged(tr("Cannot open sidecar for %1").arg(m_filePath));
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(sidecar.readAll());
    if (!doc.isObject()) {
        emit statusChanged(tr("Invalid sidecar JSON for %1").arg(m_filePath));
        return false;
    }

    if (!descriptorFromJson(doc.object(), m_descriptor)) {
        emit statusChanged(tr("Invalid descriptor for %1").arg(m_filePath));
        return false;
    }

    m_fileSize = file.size();
    m_file.setFileName(m_filePath);
    if (!m_file.open(QIODevice::ReadOnly)) {
        emit statusChanged(tr("Cannot open %1 for reading: %2").arg(m_filePath, m_file.errorString()));
        return false;
    }

    m_position = 0;
    return true;
}

void FilePlaybackModel::startPlayback()
{
    if (m_playing)
        return;

    if (m_filePath.isEmpty()) {
        emit statusChanged(tr("No file path set"));
        return;
    }

    if (!loadFile())
        return;

    int intervalMs = static_cast<int>((m_chunkSize / m_descriptor.sampleRate) * 1000.0);
    if (intervalMs < 1)
        intervalMs = 1;

    m_timer->start(intervalMs);
    m_playing = true;
    emit statusChanged(tr("Playing %1").arg(m_filePath));
}

void FilePlaybackModel::stopPlayback()
{
    if (!m_playing)
        return;

    m_timer->stop();
    m_file.close();
    m_playing = false;
    m_position = 0;
    m_output.reset();
    emit statusChanged(tr("Stopped"));
}

void FilePlaybackModel::onTimerTick()
{
    if (!m_playing)
        return;

    QByteArray chunk = m_file.read(m_chunkSize);
    if (chunk.isEmpty()) {
        // End of file - auto-stop
        stopPlayback();
        return;
    }

    m_output = std::make_shared<SampledData>(chunk, m_descriptor);
    m_position += chunk.size();
    emit dataUpdated(0);
    emit statusChanged(tr("Playing... %1/%2 bytes").arg(m_position).arg(m_fileSize));
}

void FilePlaybackModel::updateStatus(const QString &status)
{
    emit statusChanged(status);
}