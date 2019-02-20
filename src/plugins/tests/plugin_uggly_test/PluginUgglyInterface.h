#ifndef PLUGINUGGLYINTERFACE_H
#define PLUGINUGGLYINTERFACE_H

#include <QObject>
#include "plugin_global.h"
#include "QPluginInterface.h"


using namespace Daqster;


class PLUGIN_EXPORT PluginUgglyInterface:  public QPluginInterface
{
    Q_OBJECT
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    Q_PLUGIN_METADATA(IID IID_DAQSTER_PLUGIN_INTERFACE FILE "UgglyTestPlugin.json")
#endif
    Q_INTERFACES(Daqster::QPluginInterface)
public:
    PluginUgglyInterface( QObject* parent = 0);
    ~PluginUgglyInterface(  );
protected:
    virtual Daqster::QBasePluginObject* CreatePluginInternal(QObject* Parrent = nullptr);
};

#endif // PLUGINUGGLYINTERFACE_H
