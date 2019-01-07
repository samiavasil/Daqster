#ifndef PLUGINFANCYINTERFACE_H
#define PLUGINFANCYINTERFACE_H

#include <QObject>
#include "plugin_global.h"
#include "QDaqsterPluginInterface.h"


using namespace Daqster;


class PLUGIN_EXPORT PluginFancyInterface:  public QDaqsterPluginInterface
{
    Q_OBJECT
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    Q_PLUGIN_METADATA(IID IID_DAQSTER_PLUGIN_INTERFACE FILE "PluginFancyTest.json")
#endif
    Q_INTERFACES(Daqster::QDaqsterPluginInterface)
public:
    PluginFancyInterface( QObject* parent = 0);
    ~PluginFancyInterface(  );
protected:
    virtual Daqster::QBasePluginObject* CreatePluginInternal(QObject* Parrent = NULL);
};

#endif // PLUGINFANCYINTERFACE_H
