#ifndef TEMPLATEPLUGINOBJECT_H
#define TEMPLATEPLUGINOBJECT_H
#include "QBasePluginObject.h"
class QMainWindow;

using namespace Daqster;
class TemplatePluginObject: public QBasePluginObject{
    Q_OBJECT
public:
    TemplatePluginObject(QObject* Parent = NULL);
    virtual ~TemplatePluginObject();
    void SetName(const QString& name);

public slots:
    void MainWinDestroyed(QObject *obj);
    void ShowPlugins();
private:
    QMainWindow* m_Win;
};
#endif // TEMPLATEPLUGINOBJECT_H
