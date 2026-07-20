#ifndef QDEVIODISPLAY_H
#define QDEVIODISPLAY_H

#include "../../Audio/AudioCompat.h"
#include "QDevIOStreamConfig.h"

#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtNodes/NodeDelegateModel>

class NodeDataModelToQIODeviceConnector;
class GenericQDevIoConnector;
class QStackedWidget;
class QWidget;

/**
 * @brief Generic display model for QDevIO streams.
 *
 * Uses a QStackedWidget to switch between views based on stream metadata.
 * - If connector has streamConfig → auto-route by config.type
 * - If no config → show manual config panel
 * - Mixed streams → activate all matching views
 *
 * Audio-specific subclass (AudioDisplayModel) overrides for demux.
 */
class QDevIoDisplayModel : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    QDevIoDisplayModel();

    virtual
    ~QDevIoDisplayModel() override;

public:

    QString
    caption() const override
    { return QStringLiteral("QDevIo Display"); }

    bool
    captionVisible() const override
    { return false; }

    QString
    name() const override
    { return QStringLiteral("QDevIoDisplay"); }

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

    // ── View Registration ──────────────────────────────────────
    void registerView(const QString& type, QWidget* widget);
    int viewIndex(const QString& type) const;

public slots:
    void ChangeAudioConnection(QAudioDeviceInfo devInfo, QAudioFormat formatAudio);

protected:
    void handleGenericConnector(std::shared_ptr<GenericQDevIoConnector> connector);
    void showConfigPanel();

    std::shared_ptr<NodeDataModelToQIODeviceConnector> m_connector;
    QWidget* m_widget;              // QStackedWidget (the embedded widget)
    std::shared_ptr<QIODevice> m_device;
    QDevIOStreamConfig m_currentConfig;

    // ── Stacked Widget Pages ───────────────────────────────────
    QStackedWidget* m_stack = nullptr;
    QMap<QString, int> m_typeToWidget;  // "audio"→0, "video"→1, etc.
    int m_configPanelIndex = -1;        // index of manual config panel
    int m_audioViewIndex = -1;          // index of audio waveform view

    friend class AudioXYSeriesIODevice;
};

#endif // QDEVIODISPLAY_H
