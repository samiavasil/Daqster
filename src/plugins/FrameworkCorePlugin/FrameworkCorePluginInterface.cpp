#include "FrameworkCorePluginInterface.h"
#include "FrameworkCorePluginObject.h"

#include <logging/LogCategories.h>

#include <QDebug>

namespace Daqster {

FrameworkCorePluginInterface::FrameworkCorePluginInterface(QObject* parent)
    : QPluginInterface(parent)
{
    qCDebug(lcNodeEditor) << "FrameworkCorePluginInterface constructed";
}

FrameworkCorePluginInterface::~FrameworkCorePluginInterface()
{
    qCDebug(lcNodeEditor) << "FrameworkCorePluginInterface destroyed";
}

Daqster::QBasePluginObject* FrameworkCorePluginInterface::CreatePluginInternal(QObject* parent)
{
    return new Daqster::FrameworkCorePluginObject(parent);
}

} // namespace Daqster