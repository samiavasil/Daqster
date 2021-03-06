#ifndef PLUGINFANCYINTERFACE_H
#define PLUGINFANCYINTERFACE_H

#include <QObject>
#include "plugin_global.h"
#include "QPluginInterface.h"


using namespace Daqster;


class PLUGIN_EXPORT PluginFancyInterface:  public QPluginInterface
{
    Q_OBJECT
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    Q_PLUGIN_METADATA(IID IID_DAQSTER_PLUGIN_INTERFACE FILE "PluginFancyTest.json")
#endif
    Q_INTERFACES(Daqster::QPluginInterface)
public:
    PluginFancyInterface( QObject* parent = 0);
    ~PluginFancyInterface(  );
protected:
    virtual Daqster::QBasePluginObject* CreatePluginInternal(QObject* Parrent = nullptr);
};

#endif // PLUGINFANCYINTERFACE_H
