#ifndef FILEPLAYBACKMODEL_H
#define FILEPLAYBACKMODEL_H

#include "NodeDataTypes/SampledData.h"
#include "FilePlaybackWidget.h"

#include <QtNodes/NodeDelegateModel>

#include <QByteArray>
#include <QTimer>

#include <memory>

/**
 * @brief File Playback source node model (REQ-SW-PL-043).
 *
 * Thin NodeDelegateModel controller: 1 output port (SampledData "sample").
 * On Play it reads the `*.sdf` raw bytes + the `*.sdf.json` sidecar
 * (SampledStreamDescriptor), then emits the data in chunks on a QTimer whose
 * interval = chunkSize / sampleRate (real-time tempo). At the end of the file
 * playback auto-stops.
 *
 * Connection-count gating (model of SystemMonitorModel/PlutoSdrModel): the
 * timer runs only while the user pressed Play AND at least one output
 * connection exists; removing the last connection auto-stops the playback.
 */
class FilePlaybackModel : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    FilePlaybackModel();
    ~FilePlaybackModel() override;

    QString caption() const override
    { return QStringLiteral("File Playback"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("FilePlayback"); }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex port) override;

    QWidget *embeddedWidget() override;

    void outputConnectionCreated(QtNodes::ConnectionId const &) override;
    void outputConnectionDeleted(QtNodes::ConnectionId const &) override;

private slots:
    void onPlayRequested();
    void onStopRequested();
    void onPathChanged(const QString &path);
    void onTimerTick();

private:
    bool loadFile();
    void startPlayback();
    void stopPlayback();
    void updateStatus();

    FilePlaybackWidget *m_widget = nullptr;
    QTimer m_timer;
    QByteArray m_data;
    SampledStreamDescriptor m_descriptor;
    std::shared_ptr<SampledData> m_output;
    int m_chunkSize = 0;
    int m_position = 0;
    int m_connectionCount = 0;
    bool m_userStarted = false;
    bool m_loaded = false;
};

#endif // FILEPLAYBACKMODEL_H
