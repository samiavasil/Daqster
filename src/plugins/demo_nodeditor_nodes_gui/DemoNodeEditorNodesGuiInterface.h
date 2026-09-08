#ifndef DEMONODEEDITORNODESGUIINTERFACE_H
#define DEMONODEEDITORNODESGUIINTERFACE_H

#include <DaqsterCore/plugin/QPluginInterface.h>

namespace Daqster {

class DemoNodeEditorNodesGuiInterface : public QPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "Daqster.PlugIn.QPluginInterface" FILE "DemoNodeEditorNodesGuiInterface.json")

public:
    explicit DemoNodeEditorNodesGuiInterface(QObject* parent = nullptr);
    ~DemoNodeEditorNodesGuiInterface() override;

protected:
    QBasePluginObject* CreatePluginInternal(QObject* parent = nullptr) override;
};

} // namespace Daqster

#endif // DEMONODEEDITORNODESGUIINTERFACE_H