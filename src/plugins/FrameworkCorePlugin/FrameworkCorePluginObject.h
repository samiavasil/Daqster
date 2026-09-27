#pragma once

#include "QBasePluginObject.h"
#include <capabilities/INodeProvider.h>
#include <capabilities/IRuntimeHost.h>

namespace Daqster {

class HeadlessEngine;

/**
 * @brief Core framework plugin — the headless runtime host.
 *
 * Owns the HeadlessEngine and advertises it as an IRuntimeHost with
 * RuntimeMode::Headless, so the runner application can start it purely through
 * QPluginManager discovery (REQ-SW-PL-053) without linking this plugin.
 */
class FrameworkCorePluginObject : public QBasePluginObject, public INodeProvider, public IRuntimeHost
{
    Q_OBJECT
    // No Q_INTERFACES here: INodeProvider/IRuntimeHost are non-QObject interfaces

public:
    explicit FrameworkCorePluginObject(QObject* parent = nullptr);
    ~FrameworkCorePluginObject() override;

    // INodeProvider interface
    void registerNodes(QtNodes::NodeDelegateModelRegistry& registry) const override;
    void registerNodesHeadless(QtNodes::NodeDelegateModelRegistry& registry) const override;

    // IRuntimeHost interface
    RuntimeMode runtimeMode() const override;
    bool loadFlow(const QString& flowPath) override;
    void stopAllNodes() override;

    // QBasePluginObject interface
    bool Initialize() override;

protected:
    void DeInitialize() override;

private:
    /// Owned by this object, so its lifetime follows plugin shutdown.
    HeadlessEngine* m_engine = nullptr;
};

} // namespace Daqster
