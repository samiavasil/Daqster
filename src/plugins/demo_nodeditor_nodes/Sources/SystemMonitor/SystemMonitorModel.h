#ifndef SYSTEMMONITORMODEL_H
#define SYSTEMMONITORMODEL_H

#include "NodeDataTypes/SampledData.h"
#include "SystemMonitorEngine.h"
#include "SystemMonitorWidget.h"

#include <QtNodes/NodeDelegateModel>

#include <memory>

/**
 * @brief System Monitor source node model (REQ-SW-PL-041).
 *
 * Thin NodeDelegateModel controller: 1 output port (SampledData "sample"),
 * owns the SystemMonitorEngine (Linux /proc + /sys telemetry) + the
 * SystemMonitorWidget (config UI). Each poll wraps the current metric values
 * in a shared_ptr<SampledData> with a SampledStreamDescriptor (domain="system",
 * 5 FLOAT32 channels) and emits dataUpdated(0).
 *
 * Connection-count gating (model of PlutoSdrModel/VideoFileSourceNode): the
 * engine polls only while the user pressed Start AND at least one output
 * connection exists; removing the last connection auto-stops the polling
 * (clean teardown).
 */
class SystemMonitorModel : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    SystemMonitorModel();
    ~SystemMonitorModel() override;

    QString caption() const override
    { return QStringLiteral("System Monitor"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("SystemMonitor"); }

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
    void onStartRequested();
    void onStopRequested();
    void onIntervalChanged(double sec);
    void onMetricsChanged(bool cpu, bool ram, bool temp, bool network);
    void onMetricsReady(const SystemMonitorMetrics &m);
    void onErrorOccurred(const QString &message);

private:
    void updateEngineConfig();
    void setPollingEnabled(bool enabled);

    SystemMonitorEngine *m_engine = nullptr;
    SystemMonitorWidget *m_widget = nullptr;
    std::shared_ptr<SampledData> m_output;
    int m_connectionCount = 0;
    bool m_userStarted = false;
};

#endif // SYSTEMMONITORMODEL_H
