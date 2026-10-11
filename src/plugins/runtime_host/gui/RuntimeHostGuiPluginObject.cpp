#include "RuntimeHostGuiPluginObject.h"
#include "RuntimeShell.h"

#include <QtNodes/NodeDelegateModelRegistry>
#include <logging/LogCategories.h>

namespace Daqster {

RuntimeHostGuiPluginObject::RuntimeHostGuiPluginObject(QObject* parent)
    : QBasePluginObject(parent)
    , m_runtimeShell(new RuntimeShell(this))
{
}

RuntimeHostGuiPluginObject::~RuntimeHostGuiPluginObject()
{
    // m_runtimeShell is a QObject child — Qt deletes it. stopAllNodes() must be
    // called explicitly by the runner while the graph is still valid.
}

bool RuntimeHostGuiPluginObject::Initialize()
{
    qCInfo(lcNodeEditor) << "RuntimeHostGuiPlugin initialized (GUI runtime host ready)";
    return true;
}

void RuntimeHostGuiPluginObject::DeInitialize()
{
    qCInfo(lcNodeEditor) << "RuntimeHostGuiPlugin deinitialized";
}

// ── IRuntimeHost (RuntimeMode::Gui) ────────────────────────────────────────
RuntimeMode RuntimeHostGuiPluginObject::runtimeMode() const
{
    return RuntimeMode::Gui;
}

bool RuntimeHostGuiPluginObject::loadFlow(const QString& flowPath)
{
    return m_runtimeShell->RunRuntime(flowPath);
}

void RuntimeHostGuiPluginObject::stopAllNodes()
{
    m_runtimeShell->stopAllNodes();
}

bool RuntimeHostGuiPluginObject::configureControlPlane(const ControlPlaneOptions& options)
{
    // REQ-SW-PL-054: GUI mode speaks the same REST/WebSocket control plane as
    // the headless engine. RuntimeShell implements Daqster::RuntimeController
    // and owns the ControlPlaneServer.
    QString error;
    if (!m_runtimeShell->startControlPlane(options, &error)) {
        qCCritical(lcNodeEditor) << "RuntimeHostGuiPlugin: control plane rejected:" << error;
        return false;
    }
    return true;
}

// ── INodeProvider ──────────────────────────────────────────────────────────
void RuntimeHostGuiPluginObject::registerNodes(QtNodes::NodeDelegateModelRegistry& registry) const
{
    // No-op: display/output node models ship in NodeditorNodesCore, and
    // RuntimeShell::registerNodes() pulls every provider in through
    // QPluginManager::nodeProviders(). This plugin contributes the runtime host
    // and its widgets, not new node types.
    Q_UNUSED(registry)
}

void RuntimeHostGuiPluginObject::registerNodesHeadless(QtNodes::NodeDelegateModelRegistry& registry) const
{
    // Never registers anything headless: the GUI runtime is not available when
    // no display exists, which is precisely why headless mode must not see it.
    Q_UNUSED(registry)
}

} // namespace Daqster
