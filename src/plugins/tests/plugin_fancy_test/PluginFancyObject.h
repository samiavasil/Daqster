#ifndef PLUGINFANCYOBJECT_H
#define PLUGINFANCYOBJECT_H
#include "QBasePluginObject.h"
class QMainWindow;

using namespace Daqster;
class PluginFancyObject: public QBasePluginObject{
    Q_OBJECT
public:
    PluginFancyObject(QObject* Parent = nullptr);
    virtual ~PluginFancyObject();
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
#endif // PLUGINFANCYOBJECT_H
