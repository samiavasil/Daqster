#include "NodeditorNodesCoreInterface.h"
#include "NodeditorNodesCoreObject.h"

namespace Daqster {

NodeditorNodesCoreInterface::NodeditorNodesCoreInterface(QObject* parent)
    : QPluginInterface(parent)
{
}

NodeditorNodesCoreInterface::~NodeditorNodesCoreInterface()
{
}

QBasePluginObject* NodeditorNodesCoreInterface::CreatePluginInternal(QObject* parent)
{
    return new NodeditorNodesCoreObject(parent);
}

} // namespace Daqster