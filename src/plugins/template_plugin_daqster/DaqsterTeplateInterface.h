#ifndef DATAPLOTINTERFACE_H
#define DATAPLOTINTERFACE_H

#include <QObject>
#include "plugin_global.h"
#include "base/QPluginBaseInterface.h"
#include "base/QBasePluginObject.h"
#include <QMainWindow>

using namespace Daqster;

class TemplatePluginObject: public QBasePluginObject{
    Q_OBJECT
public:
    TemplatePluginObject(QObject* Parent = NULL);
    virtual ~TemplatePluginObject();

public slots:
    void MainWinDestroyed(QObject *obj);
private:
    QMainWindow* m_Win;
};

class PLUGIN_EXPORT DaqsterTeplateInterface:  public QPluginBaseInterface
{
    Q_OBJECT
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    Q_PLUGIN_METADATA(IID "Daqster.PlugIn.BaseInterface" FILE "DaqsterTeplateInterface.json")
#endif
    Q_INTERFACES(Daqster::QPluginBaseInterface)
public:
    DaqsterTeplateInterface( QObject* parent = 0);
    ~DaqsterTeplateInterface(  );
protected:
    virtual Daqster::QBasePluginObject* createPluginInternal(QObject* Parrent = NULL);
};

#endif // DATAPLOTINTERFACE_H
