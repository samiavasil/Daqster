#include "FrameworkCorePluginObject.h"
#include <HeadlessEngine.h>
#include <logging/LogCategories.h>

#include <QtNodes/NodeDelegateModelRegistry>

namespace Daqster {

FrameworkCorePluginObject::FrameworkCorePluginObject(QObject* parent)
    : QBasePluginObject(parent)
    , m_engine(new HeadlessEngine(this))
{
}

FrameworkCorePluginObject::~FrameworkCorePluginObject()
{
    // m_engine is a QObject child — Qt deletes it. stopAllNodes() must be
    // called explicitly by the runner while the graph is still valid.
}

bool FrameworkCorePluginObject::Initialize()
{
    qCInfo(lcNodeEditor) << "FrameworkCorePlugin initialized (headless runtime host ready)";
    return true;
}

void FrameworkCorePluginObject::DeInitialize()
{
    qCInfo(lcNodeEditor) << "FrameworkCorePlugin deinitialized";
}

// ── IRuntimeHost (RuntimeMode::Headless) ──────────────────────────────────
RuntimeMode FrameworkCorePluginObject::runtimeMode() const
{
    return RuntimeMode::Headless;
}

bool FrameworkCorePluginObject::loadFlow(const QString& flowPath)
{
    return m_engine->loadFlow(flowPath);
}

void FrameworkCorePluginObject::stopAllNodes()
{
    m_engine->stopAllNodes();
}

// ── INodeProvider ──────────────────────────────────────────────────────────
void FrameworkCorePluginObject::registerNodes(QtNodes::NodeDelegateModelRegistry& registry) const
{
    // No-op: the framework itself ships no node types. Node models come from
    // dedicated providers (e.g. DemoNodeEditorNodesCore), which the core and
    // GUI engines each discover through QPluginManager::nodeProviders().
    Q_UNUSED(registry)
}

void FrameworkCorePluginObject::registerNodesHeadless(QtNodes::NodeDelegateModelRegistry& registry) const
{
    // Same as registerNodes() — see above. HeadlessEngine gathers node types from
    // all discovered INodeProvider plugins, not from this one.
    Q_UNUSED(registry)
}

} // namespace Daqster
