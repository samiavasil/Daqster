#ifndef GPUMONITORMODEL_H
#define GPUMONITORMODEL_H

#include "GpuMonitorEngine.h"
#include "GpuMonitorWidget.h"
#include "NodeDataTypes/SampledData.h"

#include <QtNodes/NodeDelegateModel>

#include <memory>

/**
 * @brief GPU Monitor source node (REQ-SW-PL-045).
 *
 * A NodeDelegateModel with one output port of type SampledData ("sample").
 * Owns a GpuMonitorEngine (NVML polling) and a GpuMonitorWidget (config UI).
 * On each engine metricsReady() it wraps the metrics into a SampledData with a
 * SampledStreamDescriptor (domain="gpu", 6 FLOAT32 channels) and emits
 * dataUpdated(0). Polling is gated on output connection count (auto start/stop).
 */
class GpuMonitorModel : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    GpuMonitorModel();
    ~GpuMonitorModel() override;

    QString caption() const override
    { return QStringLiteral("GPU Monitor"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("GpuMonitor"); }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex port) override;

    QWidget *embeddedWidget() override;

    QtNodes::ConnectionPolicy portConnectionPolicy(QtNodes::PortType portType,
                                                   QtNodes::PortIndex portIndex) const override
    {
        Q_UNUSED(portType);
        Q_UNUSED(portIndex);
        return QtNodes::ConnectionPolicy::One;
    }

    void outputConnectionCreated(QtNodes::ConnectionId const &) override;
    void outputConnectionDeleted(QtNodes::ConnectionId const &) override;

private slots:
    void onStartRequested();
    void onStopRequested();
    void onIntervalChanged(double seconds);
    void onMetricsReady(const GpuMonitorEngine::Metrics &m);
    void onStatusChanged(const QString &status);
    void onErrorOccurred(const QString &msg);

private:
    void setPollingEnabled(bool enabled);
    std::shared_ptr<SampledData> buildSampledData(const GpuMonitorEngine::Metrics &m) const;

    GpuMonitorEngine *m_engine = nullptr;
    GpuMonitorWidget *m_widget = nullptr;
    std::shared_ptr<SampledData> m_lastData;
    int m_connectionCount = 0;
    bool m_userStarted = false;
};

#endif // GPUMONITORMODEL_H
