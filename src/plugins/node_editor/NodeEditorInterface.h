#ifndef DATAPLOTINTERFACE_H
#define DATAPLOTINTERFACE_H

#include <QObject>
#include "plugin_global.h"
#include "QPluginInterface.h"


using namespace Daqster;


class PLUGIN_EXPORT NodeEditorInterface:  public QPluginInterface  // skipcq: CXX-W2009
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "Daqster.PlugIn.QPluginInterface" FILE "NodeEditorInterface.json")
    Q_INTERFACES(Daqster::QPluginInterface)
public:
    NodeEditorInterface( QObject* parent = nullptr);
    ~NodeEditorInterface(  );
protected:
    virtual Daqster::QBasePluginObject* CreatePluginInternal(QObject* Parrent = nullptr);
};

#endif // DATAPLOTINTERFACE_H
