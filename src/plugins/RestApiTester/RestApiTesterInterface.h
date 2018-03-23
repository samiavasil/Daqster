#ifndef QTCOINTRADER_INTERFACE_H
#define QTCOINTRADER_INTERFACE_H

#include <QObject>
#include "plugin_global.h"
#include "QDaqsterPluginInterface.h"


using namespace Daqster;


class PLUGIN_EXPORT RestApiTesterInterface:  public QDaqsterPluginInterface
{
    Q_OBJECT
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    Q_PLUGIN_METADATA(IID IID_DAQSTER_PLUGIN_INTERFACE FILE "RestApiTester.json")
#endif
    Q_INTERFACES(Daqster::QDaqsterPluginInterface)
public:
    RestApiTesterInterface( QObject* parent = 0);
    ~RestApiTesterInterface(  );
protected:
    virtual Daqster::QBasePluginObject* CreatePluginInternal(QObject* Parrent = NULL);
};

#endif // DATAPLOTINTERFACE_H
