#ifndef DEMONODEEDITORNODESCOREINTERFACE_H
#define DEMONODEEDITORNODESCOREINTERFACE_H

#include "QPluginInterface.h"

namespace Daqster {

class NodeditorNodesCoreInterface : public QPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "Daqster.PlugIn.QPluginInterface" FILE "NodeditorNodesCoreInterface.json")

public:
    explicit NodeditorNodesCoreInterface(QObject* parent = nullptr);
    ~NodeditorNodesCoreInterface() override;

protected:
    QBasePluginObject* CreatePluginInternal(QObject* parent = nullptr) override;
};

} // namespace Daqster

#endif // DEMONODEEDITORNODESCOREINTERFACE_H