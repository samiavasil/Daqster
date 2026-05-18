#ifndef TEMPLATEPLUGINOBJECT_H
#define TEMPLATEPLUGINOBJECT_H
#include "QBasePluginObject.h"
#include <QtNodes/Definitions>

class QMainWindow;
using namespace QtNodes;
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
protected slots:
    void nodeDoubleClicked(NodeId nodeId);
private:
    QMainWindow* m_Win;
};
#endif // TEMPLATEPLUGINOBJECT_H
