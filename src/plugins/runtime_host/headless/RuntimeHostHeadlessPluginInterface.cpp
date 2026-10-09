#include "RuntimeHostHeadlessPluginInterface.h"
#include "RuntimeHostHeadlessPluginObject.h"

#include <logging/LogCategories.h>

#include <QDebug>

namespace Daqster {

RuntimeHostHeadlessPluginInterface::RuntimeHostHeadlessPluginInterface(QObject* parent)
    : QPluginInterface(parent)
{
    qCDebug(lcNodeEditor) << "RuntimeHostHeadlessPluginInterface constructed";
}

RuntimeHostHeadlessPluginInterface::~RuntimeHostHeadlessPluginInterface()
{
    qCDebug(lcNodeEditor) << "RuntimeHostHeadlessPluginInterface destroyed";
}

Daqster::QBasePluginObject* RuntimeHostHeadlessPluginInterface::CreatePluginInternal(QObject* parent)
{
    return new Daqster::RuntimeHostHeadlessPluginObject(parent);
}

} // namespace Daqster