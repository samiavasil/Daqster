#include "NodeditorNodesGuiInterface.h"
#include "NodeditorNodesGuiObject.h"

namespace Daqster {

NodeditorNodesGuiInterface::NodeditorNodesGuiInterface(QObject* parent)
    : QPluginInterface(parent)
{
}

NodeditorNodesGuiInterface::~NodeditorNodesGuiInterface()
{
}

QBasePluginObject* NodeditorNodesGuiInterface::CreatePluginInternal(QObject* parent)
{
    return new NodeditorNodesGuiObject(parent);
}

} // namespace Daqster