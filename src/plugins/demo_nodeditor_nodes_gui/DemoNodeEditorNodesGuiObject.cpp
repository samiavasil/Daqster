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
    // Create and register the NodeWidgetFactory
    m_widgetFactory = new NodeWidgetFactory(this);
    return true;
}

void DemoNodeEditorNodesGuiObject::DeInitialize()
{
    if (m_widgetFactory) {
        m_widgetFactory->deleteLater();
        m_widgetFactory = nullptr;
    }
}

} // namespace Daqster