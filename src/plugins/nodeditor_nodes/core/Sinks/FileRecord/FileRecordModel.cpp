#include "FileRecordModel.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using QtNodes::NodeDataType;

namespace {

QString endianName(SampleEndian endian)
{
    return endian == SampleEndian::BigEndian
        ? QStringLiteral("BigEndian")
        : QStringLiteral("LittleEndian");
}

QJsonObject descriptorToJson(const SampledStreamDescriptor &desc)
{
    QJsonObject obj;
    obj["sampleRate"] = desc.sampleRate;

    QJsonArray channels;
    for (const StreamChannelDescriptor &ch : desc.channels) {
        QJsonObject chObj;
        chObj["name"] = ch.name;
        chObj["type"] = sampleTypeName(ch.sampleType);
        channels.append(chObj);
    }
    obj["channels"] = channels;

    obj["endianness"] = endianName(desc.endianness);
    obj["unit"] = desc.unit;
    obj["amplitudeScale"] = desc.amplitudeScale;
    obj["amplitudeOffset"] = desc.amplitudeOffset;
    obj["domain"] = desc.domain;
    obj["deviceId"] = desc.deviceId;
    obj["sourceName"] = desc.sourceName;
    obj["firstSampleTimestamp"] = desc.firstSampleTimestamp;
    obj["expectedBufferSeconds"] = desc.expectedBufferSeconds;
    return obj;
}

} // namespace

FileRecordModel::FileRecordModel()
{
    // No widget in core model - widget is created by GUI plugin via NodeWidgetFactory
}

FileRecordModel::~FileRecordModel()
{
    // Single shutdown path: stop() closes the file (REQ-SW-PL-050).
    stop();
}

void FileRecordModel::stop()
{
    // Idempotent: stopRecording() on an already-stopped recorder is a no-op.
    stopRecording();
}

/// Start recording programmatically (runtime autoStart, REQ-SW-PL-048).
/// Delegates to the same logic as onStartRequested().
void FileRecordModel::start()
{
    startRecording();
}

QJsonObject FileRecordModel::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();
    modelJson["filePath"] = m_filePath;
    return modelJson;
}

void FileRecordModel::load(QJsonObject const &p)
{
    m_filePath = p.value("filePath").toString();
}

unsigned int FileRecordModel::nPorts(QtNodes::PortType portType) const
{
    return portType == QtNodes::PortType::In ? 1 : 0;
}

QtNodes::NodeDataType FileRecordModel::dataType(QtNodes::PortType portType,
                                                QtNodes::PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return SampledData().type();
}

std::shared_ptr<QtNodes::NodeData> FileRecordModel::outData(QtNodes::PortIndex port)
{
    Q_UNUSED(port);
    return nullptr;
}

void FileRecordModel::setInData(std::shared_ptr<QtNodes::NodeData> data,
                                QtNodes::PortIndex port)
{
    Q_UNUSED(port);

    if (!m_recording)
        return;

    auto sampled = std::dynamic_pointer_cast<SampledData>(data);
    if (!sampled)
        return;

    // Capture the descriptor from the first chunk so the sidecar reflects the
    // actual stream (sample rate, channels, domain, ...).
    if (!m_hasDescriptor) {
        m_descriptor = sampled->descriptor();
        m_hasDescriptor = true;
        writeSidecar(m_descriptor);
    }

    const QByteArray &buffer = sampled->buffer();
    if (buffer.isEmpty())
        return;

    if (m_file.write(buffer) < 0) {
        emit statusChanged(tr("Write error: %1").arg(m_file.errorString()));
        return;
    }
    m_bytesWritten += buffer.size();
    updateStatus(tr("Recording... %1 bytes").arg(m_bytesWritten));
}

// ── Widget slots (public, called by GUI widget via NodeWidgetFactory) ────────

void FileRecordModel::onStartRequested()
{
    startRecording();
}

void FileRecordModel::onStopRequested()
{
    stopRecording();
}

void FileRecordModel::onPathChanged(const QString &path)
{
    if (!m_recording) {
        m_filePath = path;
    }
    // Path change while recording is not allowed — the model keeps the file
    // handle it opened. The widget's Start button is the only way to (re)open.
}

// ── Recording helpers ───────────────────────────────────────────────────────

void FileRecordModel::startRecording()
{
    if (m_recording)
        return;

    if (m_filePath.isEmpty()) {
        emit statusChanged(tr("No file path set"));
        return;
    }

    m_file.setFileName(m_filePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        emit statusChanged(tr("Cannot open %1: %2").arg(m_filePath, m_file.errorString()));
        return;
    }

    m_bytesWritten = m_file.size();
    m_hasDescriptor = false;
    m_recording = true;
    updateStatus(tr("Recording... %1 bytes").arg(m_bytesWritten));
}

void FileRecordModel::stopRecording()
{
    if (!m_recording)
        return;

    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
    }
    m_recording = false;
    updateStatus(tr("Stopped. %1 bytes written").arg(m_bytesWritten));
}

// ── Recording helpers ───────────────────────────────────────────────────────

void FileRecordModel::writeSidecar(const SampledStreamDescriptor &desc)
{
    QFile sidecar(m_filePath + ".json");
    if (sidecar.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(descriptorToJson(desc));
        sidecar.write(doc.toJson(QJsonDocument::Indented));
    }
}

void FileRecordModel::updateStatus(const QString &status)
{
    emit statusChanged(status);
}