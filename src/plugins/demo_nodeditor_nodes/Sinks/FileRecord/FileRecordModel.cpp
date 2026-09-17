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
    m_widget = new FileRecordWidget();

    connect(m_widget, &FileRecordWidget::startRequested,
            this, &FileRecordModel::onStartRequested);
    connect(m_widget, &FileRecordWidget::stopRequested,
            this, &FileRecordModel::onStopRequested);
    connect(m_widget, &FileRecordWidget::pathChanged,
            this, &FileRecordModel::onPathChanged);
}

FileRecordModel::~FileRecordModel()
{
    stopRecording();
    m_widget = nullptr;
}

QJsonObject FileRecordModel::save() const
{
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();
    modelJson["filePath"] = m_widget->filePath();
    return modelJson;
}

void FileRecordModel::load(QJsonObject const &p)
{
    m_widget->setFilePath(p.value("filePath").toString());
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
        m_widget->setStatus(tr("Write error: %1").arg(m_file.errorString()));
        return;
    }
    m_bytesWritten += buffer.size();
    updateStatus();
}

QWidget *FileRecordModel::embeddedWidget()
{
    return m_widget;
}

// ── Widget slots ────────────────────────────────────────────────────────────

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
    Q_UNUSED(path);
    // Path change while recording is not allowed — the model keeps the file
    // handle it opened. The widget's Start button is the only way to (re)open.
}

// ── Recording helpers ───────────────────────────────────────────────────────

void FileRecordModel::startRecording()
{
    if (m_recording)
        return;

    const QString path = m_widget->filePath();
    if (path.isEmpty()) {
        m_widget->setStatus(tr("No file path set"));
        return;
    }

    m_filePath = path;
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        m_widget->setStatus(tr("Cannot open %1: %2").arg(path, m_file.errorString()));
        return;
    }

    m_bytesWritten = m_file.size();
    m_hasDescriptor = false;
    m_recording = true;
    updateStatus();
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
    updateStatus();
}

void FileRecordModel::writeSidecar(const SampledStreamDescriptor &desc)
{
    const QString sidecarPath = m_filePath + QStringLiteral(".json");
    QFile sidecar(sidecarPath);
    if (!sidecar.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_widget->setStatus(tr("Cannot write sidecar %1: %2")
                                .arg(sidecarPath, sidecar.errorString()));
        return;
    }
    const QJsonDocument doc(descriptorToJson(desc));
    sidecar.write(doc.toJson(QJsonDocument::Indented));
    sidecar.close();
}

void FileRecordModel::updateStatus()
{
    if (m_recording)
        m_widget->setStatus(tr("Recording — %1 bytes written")
                                .arg(m_bytesWritten));
    else
        m_widget->setStatus(tr("Idle — %1 bytes written").arg(m_bytesWritten));
}
