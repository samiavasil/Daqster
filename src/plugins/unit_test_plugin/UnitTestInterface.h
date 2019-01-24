#ifndef UNIT_TEST_INTERFACE_H
#define UNIT_TEST_INTERFACE_H

#include <QObject>
#include "plugin_global.h"
#include "QPluginInterface.h"


using namespace Daqster;


class PLUGIN_EXPORT UnitTestInterface:  public QPluginInterface
{
    Q_OBJECT
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    Q_PLUGIN_METADATA(IID "Daqster.PlugIn.BaseInterface" FILE "UnitTestInterface.json")
#endif
    Q_INTERFACES(Daqster::QPluginInterface)
public:
    UnitTestInterface( QObject* parent = 0);
    ~UnitTestInterface(  );
protected:
    virtual Daqster::QBasePluginObject* CreatePluginInternal(QObject* Parrent = NULL);
};

#endif // UNIT_TEST_INTERFACE_H
