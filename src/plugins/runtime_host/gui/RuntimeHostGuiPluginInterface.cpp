#include "RuntimeHostGuiPluginInterface.h"
#include "RuntimeHostGuiPluginObject.h"

#include <logging/LogCategories.h>

#include <QDebug>

namespace Daqster {

RuntimeHostGuiPluginInterface::RuntimeHostGuiPluginInterface(QObject* parent)
    : QPluginInterface(parent)
{
    qCDebug(lcNodeEditor) << "RuntimeHostGuiPluginInterface constructed";
}

RuntimeHostGuiPluginInterface::~RuntimeHostGuiPluginInterface()
{
    qCDebug(lcNodeEditor) << "RuntimeHostGuiPluginInterface destroyed";
}

Daqster::QBasePluginObject* RuntimeHostGuiPluginInterface::CreatePluginInternal(QObject* parent)
{
    return new Daqster::RuntimeHostGuiPluginObject(parent);
}

} // namespace Daqster