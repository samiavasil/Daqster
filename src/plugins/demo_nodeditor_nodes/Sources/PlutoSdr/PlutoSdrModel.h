#ifndef PLUTOSDRMODEL_H
#define PLUTOSDRMODEL_H

#include "NodeDataTypes/SampledData.h"
#include "PlutoSdrEngine.h"
#include "PlutoSdrWidget.h"

#include <QtNodes/NodeDelegateModel>

#include <memory>

/**
 * @brief PlutoSDR RX DAQ node model (REQ-SW-PL-040).
 *
 * Thin NodeDelegateModel controller: 1 output port (SampledData "sample"),
 * owns the PlutoSdrEngine (libiio streaming) + PlutoSdrWidget (config UI).
 * Each refilled IQ buffer is wrapped in a shared_ptr<SampledData> with a
 * SampledStreamDescriptor (domain="iq", channels I/Q int16) and emitted via
 * dataUpdated(0).
 *
 * Connection-count gating (model of VideoFileSourceNode): the engine streams
 * only while the user pressed Start AND at least one output connection exists;
 * removing the last connection auto-stops the stream (clean teardown).
 */
class PlutoSdrModel : public QtNodes::NodeDelegateModel
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

    QWidget *embeddedWidget() override;

    void outputConnectionCreated(QtNodes::ConnectionId const &) override;
    void outputConnectionDeleted(QtNodes::ConnectionId const &) override;

private slots:
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
    PlutoSdrWidget *m_widget = nullptr;
    std::shared_ptr<SampledData> m_output;
    int m_connectionCount = 0;
    bool m_userStarted = false;
};

#endif // PLUTOSDRMODEL_H