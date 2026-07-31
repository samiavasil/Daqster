#pragma once

#include <QObject>
#include "plugin_global.h"
#include "QPluginInterface.h"

class PLUGIN_EXPORT RequirementsManagerInterface : public Daqster::QPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "Daqster.PlugIn.QPluginInterface" FILE "RequirementsManagerInterface.json")
    Q_INTERFACES(Daqster::QPluginInterface)
public:
    RequirementsManagerInterface(QObject* parent = nullptr);
    ~RequirementsManagerInterface();

protected:
    virtual Daqster::QBasePluginObject* CreatePluginInternal(QObject* Parrent = nullptr);
};
