#include "DemoNodeEditorNodesCoreInterface.h"
#include "DemoNodeEditorNodesCoreObject.h"

namespace Daqster {

DemoNodeEditorNodesCoreInterface::DemoNodeEditorNodesCoreInterface(QObject* parent)
    : QPluginInterface(parent)
{
}

DemoNodeEditorNodesCoreInterface::~DemoNodeEditorNodesCoreInterface()
{
}

QBasePluginObject* DemoNodeEditorNodesCoreInterface::CreatePluginInternal(QObject* parent)
{
    return new DemoNodeEditorNodesCoreObject(parent);
}

} // namespace Daqster