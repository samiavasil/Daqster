#ifndef DEMONODEEDITORNODESCOREOBJECT_H
#define DEMONODEEDITORNODESCOREOBJECT_H

#include <DaqsterCore/plugin/QBasePluginObject.h>
#include <DaqsterCore/capabilities/INodeProvider.h>

namespace Daqster {

class DemoNodeEditorNodesCoreObject : public QBasePluginObject, public INodeProvider
{
    Q_OBJECT
    // Q_INTERFACES(Daqster::INodeProvider) - removed to fix moc error

public:
    explicit DemoNodeEditorNodesCoreObject(QObject* parent = nullptr);
    ~DemoNodeEditorNodesCoreObject() override;

    // INodeProvider interface
    void registerNodes(QtNodes::NodeDelegateModelRegistry& registry) const override;

    // QBasePluginObject interface
    bool Initialize() override;

protected:
    void DeInitialize() override;
};

} // namespace Daqster

#endif // DEMONODEEDITORNODESCOREOBJECT_H