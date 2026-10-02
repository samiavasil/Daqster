#pragma once

#include "QBasePluginObject.h"
#include <capabilities/INodeProvider.h>
#include <capabilities/IRuntimeHost.h>

class RuntimeShell;

/**
 * @brief GUI framework plugin — the visible runtime host.
 *
 * Owns the RuntimeShell (deembedded-widget runtime with MDI workspaces) and
 * advertises it as an IRuntimeHost with RuntimeMode::Gui, so the runner
 * application can start it purely through QPluginManager discovery
 * (REQ-SW-PL-053) without linking this plugin.
 *
 * Because this plugin is the only one that pulls in QtWidgets, keeping the
 * runner free of a link-time dependency on it is what makes the headless
 * binary free of QtWidgets too.
 */
namespace Daqster {

class FrameworkGuiPluginObject : public QBasePluginObject, public INodeProvider, public IRuntimeHost
{
    Q_OBJECT
    // No Q_INTERFACES here: INodeProvider/IRuntimeHost are non-QObject interfaces

public:
    explicit FrameworkGuiPluginObject(QObject* parent = nullptr);
    ~FrameworkGuiPluginObject() override;

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
    RuntimeShell* m_runtimeShell = nullptr;
};

} // namespace Daqster
