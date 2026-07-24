#ifndef PLUGINMAININTERFACE_H
#define PLUGINMAININTERFACE_H

#include <QObject>
#include "plugin_global.h"
#include "QPluginInterface.h"


class PLUGIN_EXPORT PluginMainInterface:  public Daqster::QPluginInterface // skipcq: CXX-W2009
{
    Q_OBJECT
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    Q_PLUGIN_METADATA(IID IID_DAQSTER_PLUGIN_INTERFACE FILE "PluginMainTest.json")
#endif
    Q_INTERFACES(Daqster::QPluginInterface)
public:
    PluginMainInterface( QObject* parent = 0);
    ~PluginMainInterface(  );
protected:
    virtual Daqster::QBasePluginObject* CreatePluginInternal(QObject* Parrent = nullptr);
};

#endif // DATAPLOTINTERFACE_H
