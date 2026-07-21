#include "MuxNode.h"
#include <GenericQDevIoConnector.h>
#include <StreamChannel.h>
#include <QDebug>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeValidationState;

MuxNode::MuxNode()
    : m_output(std::make_shared<GenericQDevIoConnector>(this))
{
}

MuxNode::~MuxNode()
{
}

QJsonObject MuxNode::save() const
{
    QJsonObject modelJson;
    modelJson["name"] = name();
    return modelJson;
}

unsigned int MuxNode::nPorts(PortType portType) const
{
    switch (portType) {
    case QtNodes::PortType::In:
        return m_inputs.size();
    case QtNodes::PortType::Out:
        return 1;
    default:
        return 0;
    }
}

NodeDataType MuxNode::dataType(PortType portType, PortIndex portIndex) const
{
    if (portType == QtNodes::PortType::In && portIndex < m_inputs.size()) {
        return {m_inputs[portIndex].type,
                m_inputs[portIndex].type};
    }
    if (portType == QtNodes::PortType::Out) {
        return {"QDevIO", "IO"};
    }
    return {QStringLiteral("unknown"), QStringLiteral("Unknown")};
}

std::shared_ptr<NodeData> MuxNode::outData(PortIndex port)
{
    if (port == 0) {
        return m_output;
    }
    return nullptr;
}

void MuxNode::setInData(std::shared_ptr<NodeData> data, PortIndex const portIndex)
{
    if (!data || portIndex < 0) return;

    // Ensure we have enough input slots
    while (m_inputs.size() <= portIndex) {
        InputPort inp;
        inp.type = "unknown";
        m_inputs.append(inp);
    }

    auto connector = std::dynamic_pointer_cast<GenericQDevIoConnector>(data);
    if (connector && connector->hasStreamConfig()) {
        m_inputs[portIndex].type = connector->streamConfig().type;
    } else {
        m_inputs[portIndex].type = "unknown";
    }
    m_inputs[portIndex].data = data;

    // Rebuild mixed payload
    MixedStreamPayload payload;
    for (const auto& inp : m_inputs) {
        if (inp.data) {
            StreamChannel ch;
            ch.type = inp.type;
            auto inpConn = std::dynamic_pointer_cast<GenericQDevIoConnector>(inp.data);
            if (inpConn) {
                ch.decoder = inpConn->payload().channels.isEmpty() ?
                    QSharedPointer<IStreamDecoder>() :
                    inpConn->payload().channels.first().decoder;
            }
            payload.channels.append(ch);
        }
    }

    m_output->setPayload(std::move(payload));

    NodeValidationState s;
    s._state = NodeValidationState::State::Valid;
    setValidationState(s);
}

QWidget* MuxNode::embeddedWidget()
{
    return nullptr; // No embedded widget
}

QtNodes::ConnectionPolicy MuxNode::portConnectionPolicy(
    PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return QtNodes::ConnectionPolicy::One;
}
