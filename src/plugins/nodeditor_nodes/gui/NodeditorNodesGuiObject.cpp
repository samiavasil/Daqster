#include "NodeditorNodesGuiObject.h"
#include "NodeWidgetFactory.h"

namespace Daqster {

NodeditorNodesGuiObject::NodeditorNodesGuiObject(QObject* parent)
    : QBasePluginObject(parent)
{
}

NodeditorNodesGuiObject::~NodeditorNodesGuiObject()
{
}

bool NodeditorNodesGuiObject::Initialize()
{
    // Create the NodeWidgetFactory and populate it with the creators for every
    // node whose model lives in nodeditor_nodes/core. Without this the
    // factory is never consulted and every core model that returns nullptr
    // from embeddedWidget() would show no controls (REQ-SW-PL-051).
    m_widgetFactory = new NodeWidgetFactory(this);
    registerDefaultWidgetCreators(m_widgetFactory);
    return true;
}

QWidget* NodeditorNodesGuiObject::createWidget(QtNodes::NodeDelegateModel* model) const
{
    if (m_widgetFactory == nullptr)
        return nullptr;
    return m_widgetFactory->createWidget(model);
}

void NodeditorNodesGuiObject::DeInitialize()
{
    if (m_widgetFactory) {
        m_widgetFactory->deleteLater();
        m_widgetFactory = nullptr;
    }
}

} // namespace Daqster