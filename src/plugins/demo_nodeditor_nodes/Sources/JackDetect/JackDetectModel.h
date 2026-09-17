#ifndef JACKDETECTMODEL_H
#define JACKDETECTMODEL_H

#include "JackDetectEngine.h"
#include "JackDetectWidget.h"
#include "NodeDataTypes/SampledData.h"

#include <QtNodes/NodeDelegateModel>

#include <memory>

/**
 * @brief Jack Detect source node (REQ-SW-PL-046).
 *
 * A NodeDelegateModel with one output port of type SampledData ("sample").
 * Owns a JackDetectEngine (HDA /proc/asound polling) and a JackDetectWidget
 * (config UI). On each engine jacksChanged() it wraps the jack states into a
 * SampledData with a SampledStreamDescriptor (domain="jack", one dynamic
 * FLOAT32 channel per jack, values 0.0/1.0) and emits dataUpdated(0).
 * Polling is gated on output connection count (auto start/stop).
 */
class JackDetectModel : public QtNodes::NodeDelegateModel
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
    void onJacksChanged(const QVector<JackDetectEngine::JackState> &jacks);
    void onStatusChanged(const QString &status);

private:
    void setPollingEnabled(bool enabled);
    std::shared_ptr<SampledData> buildSampledData(
        const QVector<JackDetectEngine::JackState> &jacks) const;

    JackDetectEngine *m_engine = nullptr;
    JackDetectWidget *m_widget = nullptr;
    std::shared_ptr<SampledData> m_lastData;
    int m_connectionCount = 0;
    bool m_userStarted = false;
};

#endif // JACKDETECTMODEL_H