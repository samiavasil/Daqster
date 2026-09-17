#ifndef PCAPMODEL_H
#define PCAPMODEL_H

#include "PcapEngine.h"
#include "PcapWidget.h"
#include "NodeDataTypes/SampledData.h"

#include <QtNodes/NodeDelegateModel>

#include <memory>
#include <queue>
#include <mutex>

/**
 * @brief pcap Packet Capture source node (REQ-SW-PL-047).
 *
 * A NodeDelegateModel with one output port of type SampledData ("packet").
 * Owns a PcapEngine (libpcap capture in worker thread) and a PcapWidget
 * (config UI). On each engine packetCaptured() it wraps the packet into a
 * SampledData with a SampledStreamDescriptor (domain="pcap", BYTES channel)
 * and emits dataUpdated(0). Capture is gated on output connection count
 * (auto start/stop) and user Start/Stop.
 */
class PcapModel : public QtNodes::NodeDelegateModel
{
    Q_OBJECT

public:
    PcapModel();
    ~PcapModel() override;

    QString caption() const override
    { return QStringLiteral("pcap Capture"); }

    bool captionVisible() const override
    { return false; }

    QString name() const override
    { return QStringLiteral("PcapCapture"); }

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
    void onInterfaceChanged(const QString &interface);
    void onFilterChanged(const QString &filter);
    void onSnaplenChanged(int snaplen);
    void onPromiscuousChanged(bool promiscuous);
    void onPacketCaptured(const PcapEngine::Packet &packet);
    void onStatusChanged(const QString &status);
    void onErrorOccurred(const QString &msg);
    void onStatsUpdated(quint64 captured, quint64 dropped, quint64 ifDropped);

private:
    void setCaptureEnabled(bool enabled);
    std::shared_ptr<SampledData> buildSampledData(const PcapEngine::Packet &pkt) const;
    void enqueuePacket(const PcapEngine::Packet &pkt);
    bool dequeuePacket(PcapEngine::Packet &pkt);

    PcapEngine *m_engine = nullptr;
    PcapWidget *m_widget = nullptr;
    std::shared_ptr<SampledData> m_lastData;

    // Thread-safe packet queue from engine thread to model (GUI) thread
    mutable std::mutex m_queueMutex;
    std::queue<PcapEngine::Packet> m_packetQueue;

    int m_connectionCount = 0;
    bool m_userStarted = false;
};

#endif // PCAPMODEL_H