#ifndef PLUGINFANCYOBJECT_H
#define PLUGINFANCYOBJECT_H
#include "QBasePluginObject.h"
class QMainWindow;

using namespace Daqster;
class PluginFancyObject: public QBasePluginObject{
    Q_OBJECT
public:
    PluginFancyObject(QObject* Parent = NULL);
    virtual ~PluginFancyObject();
    void SetName(const QString& name);

public slots:
    void MainWinDestroyed(QObject *obj);
private:
    QMainWindow* m_Win;
};
#endif // PLUGINFANCYOBJECT_H
