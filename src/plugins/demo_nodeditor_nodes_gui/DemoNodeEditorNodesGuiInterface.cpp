#include "DemoNodeEditorNodesGuiInterface.h"
#include "DemoNodeEditorNodesGuiObject.h"

namespace Daqster {

DemoNodeEditorNodesGuiInterface::DemoNodeEditorNodesGuiInterface(QObject* parent)
    : QPluginInterface(parent)
{
}

DemoNodeEditorNodesGuiInterface::~DemoNodeEditorNodesGuiInterface()
{
}

QBasePluginObject* DemoNodeEditorNodesGuiInterface::CreatePluginInternal(QObject* parent)
{
    return new DemoNodeEditorNodesGuiObject(parent);
}

} // namespace Daqster