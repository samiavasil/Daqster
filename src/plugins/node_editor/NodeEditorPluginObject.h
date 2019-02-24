#ifndef TEMPLATEPLUGINOBJECT_H
#define TEMPLATEPLUGINOBJECT_H
#include "QBasePluginObject.h"
class QMainWindow;

using namespace Daqster;
class NodeEditorPluginObject: public QBasePluginObject{
    Q_OBJECT
public:
    NodeEditorPluginObject(QObject* Parent = nullptr);
    virtual ~NodeEditorPluginObject();
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
#endif // TEMPLATEPLUGINOBJECT_H
