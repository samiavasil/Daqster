#pragma once

#include <QObject>
#include "plugin_global.h"
#include "QPluginInterface.h"

using namespace Daqster;

class PLUGIN_EXPORT DemoStandardNodesInterface : public QPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "Daqster.PlugIn.QPluginInterface" FILE "DemoStandardNodesInterface.json")
    Q_INTERFACES(Daqster::QPluginInterface)
public:
    DemoStandardNodesInterface(QObject* parent = nullptr);
    ~DemoStandardNodesInterface();

protected:
    virtual Daqster::QBasePluginObject* CreatePluginInternal(QObject* Parrent = nullptr);
};
