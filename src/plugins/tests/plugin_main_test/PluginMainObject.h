#ifndef PLUGINMAINOBJECT_H
#define PLUGINMAINOBJECT_H
#include "QBasePluginObject.h"
class QMainWindow;

using namespace Daqster;
class PluginMainObject: public QBasePluginObject{
    Q_OBJECT
public:
    PluginMainObject(QObject* Parent = nullptr);
    virtual ~PluginMainObject();
    void SetName(const QString& name);
    virtual bool Initialize();
protected:
    virtual void DeInitialize();
public slots:
    void MainWinDestroyed(QObject *obj);
protected slots:
    void ShowPlugins();
private:
    QMainWindow* m_Win;
};
#endif // PLUGINMAINOBJECT_H
