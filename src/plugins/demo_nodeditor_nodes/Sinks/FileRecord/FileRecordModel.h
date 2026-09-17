#ifndef FILERECORDMODEL_H
#define FILERECORDMODEL_H

#include "NodeDataTypes/SampledData.h"
#include "FileRecordWidget.h"

#include <QtNodes/NodeDelegateModel>

#include <QFile>

#include <memory>

/**
 * @brief File Record sink node model (REQ-SW-PL-043).
 *
 * Thin NodeDelegateModel controller: 1 input port (SampledData "sample").
 * While recording, each incoming SampledData's raw bytes are appended to a
 * `*.sdf` file; on Start a JSON sidecar `*.sdf.json` (the
 * SampledStreamDescriptor) is written; on Stop the file is flushed + closed.
 *
 * The file format is deliberately simple and debuggable: raw interleaved
 * sample bytes + a human-readable JSON sidecar (no custom binary header).
 */
class FileRecordModel : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    FileRecordModel();
    ~FileRecordModel() override;

    QString caption() const override
    { return QStringLiteral("File Record"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("FileRecord"); }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex port) override;

    QWidget *embeddedWidget() override;

private slots:
    void onStartRequested();
    void onStopRequested();
    void onPathChanged(const QString &path);

private:
    void startRecording();
    void stopRecording();
    void writeSidecar(const SampledStreamDescriptor &desc);
    void updateStatus();

    FileRecordWidget *m_widget = nullptr;
    QFile m_file;
    QString m_filePath;
    qint64 m_bytesWritten = 0;
    bool m_recording = false;
    bool m_hasDescriptor = false;
    SampledStreamDescriptor m_descriptor;
};

#endif // FILERECORDMODEL_H
