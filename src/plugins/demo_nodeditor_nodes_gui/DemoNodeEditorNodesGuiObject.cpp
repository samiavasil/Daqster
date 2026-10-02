#include "DemoNodeEditorNodesGuiObject.h"
#include "NodeWidgetFactory.h"

namespace Daqster {

DemoNodeEditorNodesGuiObject::DemoNodeEditorNodesGuiObject(QObject* parent)
    : QBasePluginObject(parent)
{
}

DemoNodeEditorNodesGuiObject::~DemoNodeEditorNodesGuiObject()
{
}

bool DemoNodeEditorNodesGuiObject::Initialize()
{
    // Create the NodeWidgetFactory and populate it with the creators for every
    // node whose model lives in demo_nodeditor_nodes_core. Without this the
    // factory is never consulted and every core model that returns nullptr
    // from embeddedWidget() would show no controls (REQ-SW-PL-051).
    m_widgetFactory = new NodeWidgetFactory(this);
    registerDefaultWidgetCreators(m_widgetFactory);
    return true;
}

QWidget* DemoNodeEditorNodesGuiObject::createWidget(QtNodes::NodeDelegateModel* model) const
{
    if (m_widgetFactory == nullptr)
        return nullptr;
    return m_widgetFactory->createWidget(model);
}

void DemoNodeEditorNodesGuiObject::DeInitialize()
{
    if (m_widgetFactory) {
        m_widgetFactory->deleteLater();
        m_widgetFactory = nullptr;
    }
}

} // namespace Daqster