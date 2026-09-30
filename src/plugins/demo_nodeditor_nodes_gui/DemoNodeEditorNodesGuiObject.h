#ifndef DEMONODEEDITORNODESGUIOBJECT_H
#define DEMONODEEDITORNODESGUIOBJECT_H

#include "QBasePluginObject.h"
#include <capabilities/INodeProvider.h>
#include <capabilities/IWidgetProvider.h>
#include "NodeWidgetFactory.h"

namespace QtNodes {
class NodeDelegateModel;
}

namespace Daqster {

class DemoNodeEditorNodesGuiObject : public QBasePluginObject
                                  , public IWidgetProvider
{
    Q_OBJECT

public:
    explicit DemoNodeEditorNodesGuiObject(QObject* parent = nullptr);
    ~DemoNodeEditorNodesGuiObject() override;

    // QBasePluginObject interface
    bool Initialize() override;

    // IWidgetProvider interface — supplies the QWidget of nodes whose model
    // lives in demo_nodeditor_nodes_core and returns nullptr from
    // embeddedWidget() (REQ-SW-PL-051).
    QWidget* createWidget(QtNodes::NodeDelegateModel* model) const override;

protected:
    void DeInitialize() override;

private:
    NodeWidgetFactory* m_widgetFactory = nullptr;
};

} // namespace Daqster

#endif // DEMONODEEDITORNODESGUIOBJECT_H