#ifndef DEMONODEEDITORNODESGUIOBJECT_H
#define DEMONODEEDITORNODESGUIOBJECT_H

#include <DaqsterCore/plugin/QBasePluginObject.h>
#include <DaqsterCore/capabilities/INodeProvider.h>
#include "NodeWidgetFactory.h"

namespace Daqster {

class DemoNodeEditorNodesGuiObject : public QBasePluginObject
{
    Q_OBJECT

public:
    explicit DemoNodeEditorNodesGuiObject(QObject* parent = nullptr);
    ~DemoNodeEditorNodesGuiObject() override;

    // QBasePluginObject interface
    bool Initialize() override;

protected:
    void DeInitialize() override;

private:
    NodeWidgetFactory* m_widgetFactory = nullptr;
};

} // namespace Daqster

#endif // DEMONODEEDITORNODESGUIOBJECT_H