#ifndef DEMONODEEDITORNODESGUIINTERFACE_H
#define DEMONODEEDITORNODESGUIINTERFACE_H

#include "QPluginInterface.h"

namespace Daqster {

class NodeditorNodesGuiInterface : public QPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "Daqster.PlugIn.QPluginInterface" FILE "NodeditorNodesGuiInterface.json")

public:
    explicit NodeditorNodesGuiInterface(QObject* parent = nullptr);
    ~NodeditorNodesGuiInterface() override;

protected:
    QBasePluginObject* CreatePluginInternal(QObject* parent = nullptr) override;
};

} // namespace Daqster

#endif // DEMONODEEDITORNODESGUIINTERFACE_H