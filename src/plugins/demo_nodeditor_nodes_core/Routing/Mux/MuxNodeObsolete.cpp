#include "MuxNodeObsolete.h"
#include <GenericQDevIoConnectorObsolete.h>
#include <StreamChannelObsolete.h>
#include <QDebug>

using QtNodes::PortType;
using QtNodes::PortIndex;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeValidationState;

MuxNodeObsolete::MuxNodeObsolete()
    : m_output(std::make_shared<GenericQDevIoConnectorObsolete>(this))
{
}

MuxNodeObsolete::~MuxNodeObsolete()
{
}

QJsonObject MuxNodeObsolete::save() const
{
    QJsonObject modelJson;
    modelJson["name"] = name();
    return modelJson;
}

unsigned int MuxNodeObsolete::nPorts(PortType portType) const
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

NodeDataType MuxNodeObsolete::dataType(PortType portType, PortIndex portIndex) const
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

std::shared_ptr<NodeData> MuxNodeObsolete::outData(PortIndex port)
{
    if (port == 0) {
        return m_output;
    }
    return nullptr;
}

void MuxNodeObsolete::setInData(std::shared_ptr<NodeData> data, PortIndex const portIndex)
{
    if (!data || portIndex < 0) return;

    // Ensure we have enough input slots
    while (m_inputs.size() <= portIndex) {
        InputPort inp;
        inp.type = "unknown";
        m_inputs.append(inp);
    }

    auto connector = std::dynamic_pointer_cast<GenericQDevIoConnectorObsolete>(data);
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
            StreamChannelObsolete ch;
            ch.type = inp.type;
            auto inpConn = std::dynamic_pointer_cast<GenericQDevIoConnectorObsolete>(inp.data);
            if (inpConn) {
                ch.decoder = inpConn->payload().channels.isEmpty() ?
                    QSharedPointer<IStreamDecoderObsolete>() :
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

QWidget* MuxNodeObsolete::embeddedWidget()
{
    return nullptr; // No embedded widget
}

QtNodes::ConnectionPolicy MuxNodeObsolete::portConnectionPolicy(
    PortType portType, PortIndex portIndex) const
{
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    return QtNodes::ConnectionPolicy::One;
}
