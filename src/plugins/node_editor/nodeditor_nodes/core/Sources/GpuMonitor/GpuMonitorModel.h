#ifndef GPUMONITORMODEL_H
#define GPUMONITORMODEL_H

#include "nodeditor_nodes_core_export.h"

#include "GpuMonitorEngine.h"
#include "NodeDataTypes/SampledData.h"
#include "capabilities/INodeConfigSchema.h"
#include "shared/IStoppable.h"
#include "shared/IStartable.h"

#include <QtNodes/NodeDelegateModel>

#include <memory>

/**
 * @brief GPU Monitor source node (REQ-SW-PL-045).
 *
 * A NodeDelegateModel with one output port of type SampledData ("sample").
 * Owns a GpuMonitorEngine (NVML polling). On each engine metricsReady() it
 * wraps the metrics into a SampledData with a SampledStreamDescriptor
 * (domain="gpu", 6 FLOAT32 channels) and emits dataUpdated(0). Polling is
 * gated on output connection count (auto start/stop).
 *
 * The GUI widget is provided by the GUI plugin via NodeWidgetFactory.
 * Core model has no QtWidgets dependency — returns nullptr from embeddedWidget().
 */
class NODEDITOR_NODES_CORE_EXPORT GpuMonitorModel : public QtNodes::NodeDelegateModel,
                                                    public Daqster::IStoppable,
                                                    public Daqster::IStartable,
                                                    public Daqster::INodeConfigSchema
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

    /// Draft-7 fragment describing exactly the object `save()` emits
    /// (REQ-SW-PL-091). Each property carries `x-restart`: `true` when a running
    /// node cannot pick the new value up and needs stop() + start(), `false`
    /// when the value takes effect live. The control plane diffs the config
    /// before/after a write and answers `restartRequired` from this marker.
    QJsonObject configSchema() const override;

    unsigned int nPorts(QtNodes::PortType portType) const override;

    QtNodes::NodeDataType dataType(QtNodes::PortType portType,
                                   QtNodes::PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex port) override;

    void setInData(std::shared_ptr<QtNodes::NodeData> data,
                   QtNodes::PortIndex port) override;

    QWidget *embeddedWidget() override { return nullptr; }

    /// Stop the polling timer + nvmlShutdown. Idempotent — safe to call
    /// multiple times (REQ-SW-PL-050).
    void stop() override;

    /// Start polling programmatically (runtime autoStart, REQ-SW-PL-048).
    void start() override;

    /// Out = Many: every poll builds a NEW `SampledData` (buildSampledData())
    /// and the graph hands that one shared_ptr to every input port; nothing
    /// downstream mutates it, so several consumers may read the same readout.
    QtNodes::ConnectionPolicy portConnectionPolicy(QtNodes::PortType portType,
                                                   QtNodes::PortIndex portIndex) const override
    {
        Q_UNUSED(portIndex);
        return portType == QtNodes::PortType::Out ? QtNodes::ConnectionPolicy::Many
                                                  : QtNodes::ConnectionPolicy::One;
    }

    void outputConnectionCreated(QtNodes::ConnectionId const &) override;
    void outputConnectionDeleted(QtNodes::ConnectionId const &) override;

    // Accessor for GUI widget factory
    GpuMonitorEngine* engine() const { return m_engine; }

public slots:
    // Public slots called by GUI widget via NodeWidgetFactory
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
    std::shared_ptr<SampledData> m_lastData;
    int m_connectionCount = 0;
    bool m_userStarted = false;
};

#endif // GPUMONITORMODEL_H
