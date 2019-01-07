#ifndef QTCOINTRADER_PLUGINOBJECT_H
#define QTCOINTRADER_PLUGINOBJECT_H
#include "QBasePluginObject.h"
class QMainWindow;

using namespace Daqster;
class RestApiTesterPluginObject: public QBasePluginObject{
    Q_OBJECT
public:
    RestApiTesterPluginObject(QObject* Parent = NULL);
    virtual ~RestApiTesterPluginObject();
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
#endif // TEMPLATEPLUGINOBJECT_H
