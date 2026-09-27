#include "FrameworkGuiPluginInterface.h"
#include "FrameworkGuiPluginObject.h"

#include <logging/LogCategories.h>

#include <QDebug>

namespace Daqster {

FrameworkGuiPluginInterface::FrameworkGuiPluginInterface(QObject* parent)
    : QPluginInterface(parent)
{
    qCDebug(lcNodeEditor) << "FrameworkGuiPluginInterface constructed";
}

FrameworkGuiPluginInterface::~FrameworkGuiPluginInterface()
{
    qCDebug(lcNodeEditor) << "FrameworkGuiPluginInterface destroyed";
}

Daqster::QBasePluginObject* FrameworkGuiPluginInterface::CreatePluginInternal(QObject* parent)
{
    return new Daqster::FrameworkGuiPluginObject(parent);
}

} // namespace Daqster