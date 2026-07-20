#include "GenericQDevIoConnector.h"
#include <QIODevice>
#include <QDebug>

GenericQDevIoConnector::GenericQDevIoConnector(QtNodes::NodeDelegateModel* model)
    : NodeDataModelToQIODeviceConnector(model)
{
}

void GenericQDevIoConnector::setIODevice(std::shared_ptr<QIODevice> device)
{
    m_device = device;
}

std::shared_ptr<QIODevice> GenericQDevIoConnector::ioDevice() const
{
    return m_device;
}

void GenericQDevIoConnector::setStreamConfig(QDevIOStreamConfig config)
{
    m_config = config;
    m_hasConfig = true;
}

bool GenericQDevIoConnector::hasStreamConfig() const
{
    return m_hasConfig;
}

QDevIOStreamConfig GenericQDevIoConnector::streamConfig() const
{
    return m_config;
}

void GenericQDevIoConnector::setPayload(MixedStreamPayload payload)
{
    m_payload = std::move(payload);
}

bool GenericQDevIoConnector::isMixed() const
{
    return m_payload.channels.size() > 1;
}

MixedStreamPayload& GenericQDevIoConnector::payload()
{
    return m_payload;
}

void GenericQDevIoConnector::ConnectModels(QtNodes::NodeDelegateModel* dst_model)
{
    Q_UNUSED(dst_model)
    // Generic connector — display handles the connection logic
    // via setInData() and reading the metadata
}

QtNodes::NodeDataType GenericQDevIoConnector::type() const
{
    return {"QDevIO", "IO"};
}
