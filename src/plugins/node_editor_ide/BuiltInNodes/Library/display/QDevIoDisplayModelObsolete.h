#ifndef QDEVIODISPLAYOBSOLETE_H
#define QDEVIODISPLAYOBSOLETE_H

#include "AudioCompat.h"
#include "QDevIOStreamConfigObsolete.h"

#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtNodes/NodeDelegateModel>
#include <memory>

class NodeDataModelToQIODeviceConnectorObsolete;
class GenericQDevIoConnectorObsolete;
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
 * Audio-specific subclass (AudioDisplayModelObsolete) overrides for demux.
 *
 * @note Renamed to *_obsolete (REQ-SW-PL-023 §7) — implementation unchanged.
 *       Kept working until the QDevIO display world is deleted at the very end.
 *       Registered under its new name AND aliased under the old "QDevIoDisplay"
 *       key so old saved graphs still load.
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

    // ── View Registration ──────────────────────────────────────
    void registerView(const QString& type, QWidget* widget);
    int viewIndex(const QString& type) const;

public slots:
    void ChangeAudioConnection(QAudioDeviceInfo devInfo, QAudioFormat formatAudio);

protected:
    void handleGenericConnector(std::shared_ptr<GenericQDevIoConnectorObsolete> connector);
    void showConfigPanel();

    std::shared_ptr<NodeDataModelToQIODeviceConnectorObsolete> m_connector;
    std::shared_ptr<QIODevice> m_device;
    QDevIOStreamConfigObsolete m_currentConfig;

    // ── Stacked Widget Pages ───────────────────────────────────
    std::unique_ptr<QStackedWidget> m_stack;
    QMap<QString, int> m_typeToWidget;  // "audio"→0, "video"→1, etc.
    int m_configPanelIndex = -1;        // index of manual config panel
    int m_audioViewIndex = -1;          // index of audio waveform view
};

#endif // QDEVIODISPLAYOBSOLETE_H
