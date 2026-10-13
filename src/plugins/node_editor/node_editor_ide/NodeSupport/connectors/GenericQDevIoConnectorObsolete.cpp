#include "GenericQDevIoConnectorObsolete.h"
#include <QIODevice>
#include <QDebug>

GenericQDevIoConnectorObsolete::GenericQDevIoConnectorObsolete(QtNodes::NodeDelegateModel* model)
    : NodeDataModelToQIODeviceConnectorObsolete(model)
{
}

void GenericQDevIoConnectorObsolete::setIODevice(std::shared_ptr<QIODevice> device)
{
    m_device = device;
}

std::shared_ptr<QIODevice> GenericQDevIoConnectorObsolete::ioDevice() const
{
    return m_device;
}

void GenericQDevIoConnectorObsolete::setStreamConfig(QDevIOStreamConfigObsolete config)
{
    m_config = config;
    m_hasConfig = true;
}

bool GenericQDevIoConnectorObsolete::hasStreamConfig() const
{
    return m_hasConfig;
}

QDevIOStreamConfigObsolete GenericQDevIoConnectorObsolete::streamConfig() const
{
    return m_config;
}

void GenericQDevIoConnectorObsolete::setPayload(MixedStreamPayload payload)
{
    m_payload = std::move(payload);
}

bool GenericQDevIoConnectorObsolete::isMixed() const
{
    return m_payload.channels.size() > 1;
}

MixedStreamPayload& GenericQDevIoConnectorObsolete::payload()
{
    return m_payload;
}

void GenericQDevIoConnectorObsolete::ConnectModels(QtNodes::NodeDelegateModel* dst_model)
{
    Q_UNUSED(dst_model)
    // Generic connector — display handles the connection logic
    // via setInData() and reading the metadata
}

QtNodes::NodeDataType GenericQDevIoConnectorObsolete::type() const
{
    return {"QDevIO", "IO"};
}
