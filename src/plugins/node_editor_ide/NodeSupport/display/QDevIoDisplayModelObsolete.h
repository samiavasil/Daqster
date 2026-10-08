#ifndef QDEVIODISPLAYOBSOLETE_H
#define QDEVIODISPLAYOBSOLETE_H

#include "AudioCompat.h"
#include "QDevIOStreamConfigObsolete.h"

#include <QtCore/QObject>
#include <QtNodes/NodeDelegateModel>
#include <memory>

class NodeDataModelToQIODeviceConnectorObsolete;
class GenericQDevIoConnectorObsolete;
class QWidget;

/**
 * @brief Generic display model for QDevIO streams (core side, REQ-SW-PL-056).
 *
 * Model-only: routes QDevIO connector data, owns the audio device
 * (XYSeriesIODeviceObsolete) and exposes the stream config. NO widgets — the
 * waveform/filter view (QDevioDisplayModelUiObsolete) moved to the GUI plugin
 * (demo_nodeditor_nodes_gui) and is created by NodeWidgetFactory, which
 * connects device bufferReady + audioConnectionChanged to the view.
 * embeddedWidget() returns nullptr (REQ-SW-PL-051 model/gui pattern).
 *
 * Audio-specific subclass (AudioDisplayModelObsolete) overrides for demux.
 *
 * @note Renamed to *_obsolete (REQ-SW-PL-023 §7) — kept working until the
 *       QDevIO display world is deleted at the very end.
 */
class QDevIoDisplayModelObsolete : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    QDevIoDisplayModelObsolete();

    virtual
    ~QDevIoDisplayModelObsolete() override;

public:

    QString
    caption() const override
    { return QStringLiteral("QDevIo Display (obsolete)"); }

    bool
    captionVisible() const override
    { return false; }

    QString
    name() const override
    { return QStringLiteral("QDevIoDisplayObsolete"); }

    QJsonObject
    save() const override;

public:

    unsigned int
    nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType
    dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData>
    outData(QtNodes::PortIndex const port) override;

    void
    setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex const portIndex) override;

    QWidget *
    embeddedWidget() override;

    std::shared_ptr<QIODevice> device() const;

public slots:
    void ChangeAudioConnection(QAudioDeviceInfo devInfo, QAudioFormat formatAudio);

signals:
    /// Emitted when the audio format changes (legacy ChangeAudioConnection
    /// path). The GUI view (QDevioDisplayModelUiObsolete) sets its series
    /// count / switches to the audio page from this (REQ-SW-PL-056 split).
    void audioConnectionChanged(const QAudioFormat &format);

protected:
    void handleGenericConnector(std::shared_ptr<GenericQDevIoConnectorObsolete> connector);

    std::shared_ptr<NodeDataModelToQIODeviceConnectorObsolete> m_connector;
    std::shared_ptr<QIODevice> m_device;
    QDevIOStreamConfigObsolete m_currentConfig;
};

#endif // QDEVIODISPLAYOBSOLETE_H
