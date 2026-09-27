#include "FrameworkGuiPluginObject.h"
#include "RuntimeShell.h"

#include <QtNodes/NodeDelegateModelRegistry>
#include <logging/LogCategories.h>

// Video display widget used by the GUI runtime's deembedded VideoOutput windows.
#include <Sources/Video/VideoGLBlitWidget.h>

namespace Daqster {

FrameworkGuiPluginObject::FrameworkGuiPluginObject(QObject* parent)
    : QBasePluginObject(parent)
    , m_runtimeShell(new RuntimeShell(this))
{
}

FrameworkGuiPluginObject::~FrameworkGuiPluginObject()
{
    // m_runtimeShell is a QObject child — Qt deletes it. stopAllNodes() must be
    // called explicitly by the runner while the graph is still valid.
}

bool FrameworkGuiPluginObject::Initialize()
{
    qCInfo(lcNodeEditor) << "FrameworkGuiPlugin initialized (GUI runtime host ready)";
    return true;
}

void FrameworkGuiPluginObject::DeInitialize()
{
    qCInfo(lcNodeEditor) << "FrameworkGuiPlugin deinitialized";
}

// ── IRuntimeHost (RuntimeMode::Gui) ────────────────────────────────────────
RuntimeMode FrameworkGuiPluginObject::runtimeMode() const
{
    return RuntimeMode::Gui;
}

bool FrameworkGuiPluginObject::loadFlow(const QString& flowPath)
{
    return m_runtimeShell->RunRuntime(flowPath);
}

void FrameworkGuiPluginObject::stopAllNodes()
{
    m_runtimeShell->stopAllNodes();
}

// ── INodeProvider ──────────────────────────────────────────────────────────
void FrameworkGuiPluginObject::registerNodes(QtNodes::NodeDelegateModelRegistry& registry) const
{
    // No-op: display/output node models ship in DemoNodeEditorNodesCore, and
    // RuntimeShell::registerNodes() pulls every provider in through
    // QPluginManager::nodeProviders(). This plugin contributes the runtime host
    // and its widgets, not new node types.
    Q_UNUSED(registry)
}

void FrameworkGuiPluginObject::registerNodesHeadless(QtNodes::NodeDelegateModelRegistry& registry) const
{
    // Never registers anything headless: the GUI runtime is not available when
    // no display exists, which is precisely why headless mode must not see it.
    Q_UNUSED(registry)
}

} // namespace Daqster
