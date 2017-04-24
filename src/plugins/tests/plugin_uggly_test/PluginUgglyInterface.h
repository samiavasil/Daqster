#ifndef PLUGINUGGLYINTERFACE_H
#define PLUGINUGGLYINTERFACE_H

#include <QObject>
#include "plugin_global.h"
#include "QPluginObjectsInterface.h"


using namespace Daqster;


class PLUGIN_EXPORT PluginUgglyInterface:  public QPluginObjectsInterface
{
    Q_OBJECT
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    Q_PLUGIN_METADATA(IID "Daqster.PlugIn.BaseInterface" FILE "UgglyTestPlugin.json")
#endif
    Q_INTERFACES(Daqster::QPluginObjectsInterface)
public:
    PluginUgglyInterface( QObject* parent = 0);
    ~PluginUgglyInterface(  );
protected:
    virtual Daqster::QBasePluginObject* CreatePluginInternal(QObject* Parrent = NULL);
};

#endif // PLUGINUGGLYINTERFACE_H
