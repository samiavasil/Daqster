#ifndef UNIT_TEST_PLUGINOBJECT_H
#define UNIT_TEST_PLUGINOBJECT_H
#include "QBasePluginObject.h"
class QMainWindow;

using namespace Daqster;
class UnitTestPluginObject: public QBasePluginObject{
    Q_OBJECT
public:
    UnitTestPluginObject(QObject* Parent = NULL);
    virtual ~UnitTestPluginObject();
    void SetName(const QString& name);
    virtual bool Initialize();
protected:
    virtual void DeInitialize();
public slots:
    void MainWinDestroyed(QObject *obj);
    void ShowPlugins();
private:
    QMainWindow* m_Win;
};
#endif // UNIT_TEST_PLUGINOBJECT_H
