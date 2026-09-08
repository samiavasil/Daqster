#ifndef PLUTOSDRMODEL_H
#define PLUTOSDRMODEL_H

#include "NodeDataTypes/SampledData.h"
#include "PlutoSdrEngine.h"
#include "shared/IStoppable.h"
#include "shared/IStartable.h"

#include <QtNodes/NodeDelegateModel>

#include <memory>

/**
 * @brief PlutoSDR RX DAQ node model (REQ-SW-PL-040).
 *
 * Thin NodeDelegateModel controller: 1 output port (SampledData "sample"),
 * owns the PlutoSdrEngine (libiio streaming). Each refilled IQ buffer is
 * wrapped in a shared_ptr<SampledData> with a SampledStreamDescriptor
 * (domain="iq", channels I/Q int16) and emitted via dataUpdated(0).
 *
 * Connection-count gating (model of VideoFileSourceNode): the engine streams
 * only while the user pressed Start AND at least one output connection exists;
 * removing the last connection auto-stops the stream (clean teardown).
 *
 * The GUI widget is provided by the GUI plugin via NodeWidgetFactory.
 * Core model has no QtWidgets dependency — returns nullptr from embeddedWidget().
 */
class PlutoSdrModel : public QtNodes::NodeDelegateModel, public Daqster::IStoppable, public Daqster::IStartable
{
    Q_OBJECT

public:
    PlutoSdrModel();
    ~PlutoSdrModel() override;

    QString caption() const override
    { return QStringLiteral("PlutoSDR RX"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("PlutoSdr"); }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex port) override;

    QWidget *embeddedWidget() override { return nullptr; }

    /// Stop the stream thread cleanly. Idempotent — safe to call multiple
    /// times (REQ-SW-PL-050).
    void stop() override;

    /// Start streaming programmatically (runtime autoStart, REQ-SW-PL-048).
    void start() override;

    void outputConnectionCreated(QtNodes::ConnectionId const &) override;
    void outputConnectionDeleted(QtNodes::ConnectionId const &) override;

    // Accessor for GUI widget factory
    PlutoSdrEngine* engine() const { return m_engine; }

signals:
    void statusChanged(const QString &status);

public slots:
    // Public slots called by GUI widget via NodeWidgetFactory
    void onStartRequested();
    void onStopRequested();
    void onConfigChanged();
    void onSamplesReady(const QByteArray &buffer, double sampleRateHz, int channels);
    void onStatusChanged(const QString &status);
    void onErrorOccurred(const QString &message);

private:
    void updateEngineConfig();
    void setStreamingEnabled(bool enabled);

    PlutoSdrEngine *m_engine = nullptr;
    std::shared_ptr<SampledData> m_output;
    int m_connectionCount = 0;
    bool m_userStarted = false;
};

#endif // PLUTOSDRMODEL_H