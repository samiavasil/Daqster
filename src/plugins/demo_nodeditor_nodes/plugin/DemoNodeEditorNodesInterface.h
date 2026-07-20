#pragma once

#include <QObject>
#include "plugin_global.h"
#include "QPluginInterface.h"

class PLUGIN_EXPORT DemoNodeEditorNodesInterface : public Daqster::QPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "Daqster.PlugIn.QPluginInterface" FILE "DemoNodeEditorNodesInterface.json")
    Q_INTERFACES(Daqster::QPluginInterface)
public:
    DemoNodeEditorNodesInterface(QObject* parent = nullptr);
    ~DemoNodeEditorNodesInterface();

protected:
    virtual Daqster::QBasePluginObject* CreatePluginInternal(QObject* Parrent = nullptr);
};
