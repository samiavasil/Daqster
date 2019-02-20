#ifndef DATAPLOTINTERFACE_H
#define DATAPLOTINTERFACE_H

#include <QObject>
#include "plugin_global.h"
#include "QPluginInterface.h"


using namespace Daqster;


class PLUGIN_EXPORT DaqsterTemplateInterface:  public QPluginInterface
{
    Q_OBJECT
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    Q_PLUGIN_METADATA(IID "Daqster.PlugIn.BaseInterface" FILE "DaqsterTeplateInterface.json")
#endif
    Q_INTERFACES(Daqster::QPluginInterface)
public:
    DaqsterTemplateInterface( QObject* parent = 0);
    ~DaqsterTemplateInterface(  );
protected:
    virtual Daqster::QBasePluginObject* CreatePluginInternal(QObject* Parrent = nullptr);
};

#endif // DATAPLOTINTERFACE_H
