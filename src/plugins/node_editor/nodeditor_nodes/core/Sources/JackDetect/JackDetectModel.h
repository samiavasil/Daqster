#ifndef JACKDETECTMODEL_H
#define JACKDETECTMODEL_H

#include "nodeditor_nodes_core_export.h"

#include "JackDetectEngine.h"
#include "NodeDataTypes/SampledData.h"
#include "capabilities/INodeConfigSchema.h"
#include "shared/IStoppable.h"
#include "shared/IStartable.h"

#include <QtNodes/NodeDelegateModel>

#include <memory>

/**
 * @brief Jack Detect source node (REQ-SW-PL-046).
 *
 * A NodeDelegateModel with one output port of type SampledData ("sample").
 * Owns a JackDetectEngine (HDA /proc/asound polling). On each engine
 * jacksChanged() it wraps the jack states into a SampledData with a
 * SampledStreamDescriptor (domain="jack", one dynamic FLOAT32 channel per
 * jack, values 0.0/1.0) and emits dataUpdated(0). Polling is gated on output
 * connection count (auto start/stop).
 *
 * The GUI widget is provided by the GUI plugin via NodeWidgetFactory.
 * Core model has no QtWidgets dependency — returns nullptr from embeddedWidget().
 */
class NODEDITOR_NODES_CORE_EXPORT JackDetectModel : public QtNodes::NodeDelegateModel,
                                                    public Daqster::IStoppable,
                                                    public Daqster::IStartable,
                                                    public Daqster::INodeConfigSchema
{
    Q_OBJECT

public:
    JackDetectModel();
    ~JackDetectModel() override;

    QString caption() const override
    { return QStringLiteral("Jack Detect"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("JackDetect"); }

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

    /// Stop the polling timer. Idempotent — safe to call multiple times
    /// (REQ-SW-PL-050).
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
    JackDetectEngine* engine() const { return m_engine; }

public slots:
    // Public slots called by GUI widget via NodeWidgetFactory
    void onStartRequested();
    void onStopRequested();
    void onIntervalChanged(double seconds);
    void onJacksChanged(const QVector<JackDetectEngine::JackState> &jacks);
    void onStatusChanged(const QString &status);

private:
    void setPollingEnabled(bool enabled);
    std::shared_ptr<SampledData> buildSampledData(
        const QVector<JackDetectEngine::JackState> &jacks) const;

    JackDetectEngine *m_engine = nullptr;
    std::shared_ptr<SampledData> m_lastData;
    int m_connectionCount = 0;
    bool m_userStarted = false;
};

#endif // JACKDETECTMODEL_H