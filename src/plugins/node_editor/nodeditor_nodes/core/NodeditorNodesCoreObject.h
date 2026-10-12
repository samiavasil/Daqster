#ifndef DEMONODEEDITORNODESCOREOBJECT_H
#define DEMONODEEDITORNODESCOREOBJECT_H

#include "QBasePluginObject.h"
#include <capabilities/INodeProvider.h>

namespace Daqster {

class NodeditorNodesCoreObject : public QBasePluginObject, public INodeProvider
{
    Q_OBJECT
    // No Q_INTERFACES here: INodeProvider is a non-QObject interface, so it
    // cannot be listed. Discovery uses dynamic_cast (QPluginManager::nodeProviders).

public:
    explicit NodeditorNodesCoreObject(QObject* parent = nullptr);
    ~NodeditorNodesCoreObject() override;

    // INodeProvider interface
    void registerNodes(QtNodes::NodeDelegateModelRegistry& registry) const override;
    void registerNodesHeadless(QtNodes::NodeDelegateModelRegistry& registry) const override;

    // QBasePluginObject interface
    bool Initialize() override;

protected:
    void DeInitialize() override;
};

} // namespace Daqster

#endif // DEMONODEEDITORNODESCOREOBJECT_H