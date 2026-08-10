#include "DemuxNodeObsolete.h"
#include <GenericQDevIoConnectorObsolete.h>
#include <StreamChannelObsolete.h>
#include <QDebug>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeValidationState;

DemuxNodeObsolete::DemuxNodeObsolete()
{
}

DemuxNodeObsolete::~DemuxNodeObsolete()
{
}

QJsonObject DemuxNodeObsolete::save() const
{
    QJsonObject modelJson;
    modelJson["name"] = name();
    return modelJson;
}

unsigned int DemuxNodeObsolete::nPorts(PortType portType) const
{
    switch (portType) {
    case QtNodes::PortType::In:
        return 1;
    case QtNodes::PortType::Out:
        return m_outputs.size();
    default:
        return 0;
    }
}

NodeDataType DemuxNodeObsolete::dataType(PortType portType, PortIndex portIndex) const
{
    if (portType == QtNodes::PortType::In) {
        return {"QDevIO", "IO"};
    }
    if (portType == QtNodes::PortType::Out && portIndex < m_outputs.size()) {
        return {m_outputs[portIndex].type,
                m_outputs[portIndex].type};
    }
    return {QStringLiteral("unknown"), QStringLiteral("Unknown")};
}

std::shared_ptr<NodeData> DemuxNodeObsolete::outData(PortIndex port)
{
    if (port < m_outputs.size()) {
        return m_outputs[port].data;
    }
    return nullptr;
}

void DemuxNodeObsolete::setInData(std::shared_ptr<NodeData> data, PortIndex const portIndex)
{
    if (portIndex != 0 || !data) return;

    auto connector = std::dynamic_pointer_cast<GenericQDevIoConnectorObsolete>(data);
    if (!connector) {
        NodeValidationState s;
        s._state = NodeValidationState::State::Warning;
        s._stateMessage = "Expected GenericQDevIoConnectorObsolete";
        setValidationState(s);
        return;
    }

    m_input = data;
    m_outputs.clear();

    if (connector->isMixed()) {
        // Extract each channel as a separate output
        auto& payload = connector->payload();
        for (const auto& ch : payload.channels) {
            OutputPort out;
            out.type = ch.type;
            // Create a connector for each output channel
            auto outConnector = std::make_shared<GenericQDevIoConnectorObsolete>(this);
            outConnector->setIODevice(connector->ioDevice());
            out.data = outConnector;
            m_outputs.append(out);
        }
    } else {
        // Single type — just pass through
        OutputPort out;
        out.type = connector->hasStreamConfig() ?
            connector->streamConfig().type : "unknown";
        out.data = data;
        m_outputs.append(out);
    }

    NodeValidationState s;
    s._state = NodeValidationState::State::Valid;
    setValidationState(s);
}

QWidget* DemuxNodeObsolete::embeddedWidget()
{
    return nullptr; // No embedded widget
}

QtNodes::ConnectionPolicy DemuxNodeObsolete::portConnectionPolicy(
    PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return QtNodes::ConnectionPolicy::Many;
}
