#ifndef PLUGINUGGLYOBJECT_H
#define PLUGINUGGLYOBJECT_H
#include "QBasePluginObject.h"
class QMainWindow;

using namespace Daqster;
class PluginUgglyObject: public QBasePluginObject{
    Q_OBJECT
public:
    PluginUgglyObject(QObject* Parent = NULL);
    virtual ~PluginUgglyObject();
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
#endif // PLUGINUGGLYOBJECT_H
