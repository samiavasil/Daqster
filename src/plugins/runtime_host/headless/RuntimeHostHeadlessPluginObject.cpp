#include "RuntimeHostHeadlessPluginObject.h"
#include <HeadlessEngine.h>
#include <logging/LogCategories.h>

#include <QtNodes/NodeDelegateModelRegistry>

namespace Daqster {

RuntimeHostHeadlessPluginObject::RuntimeHostHeadlessPluginObject(QObject* parent)
    : QBasePluginObject(parent)
    , m_engine(new HeadlessEngine(this))
{
}

RuntimeHostHeadlessPluginObject::~RuntimeHostHeadlessPluginObject()
{
    // m_engine is a QObject child — Qt deletes it. stopAllNodes() must be
    // called explicitly by the runner while the graph is still valid.
}

bool RuntimeHostHeadlessPluginObject::Initialize()
{
    qCInfo(lcNodeEditor) << "RuntimeHostHeadlessPlugin initialized (headless runtime host ready)";
    return true;
}

void RuntimeHostHeadlessPluginObject::DeInitialize()
{
    qCInfo(lcNodeEditor) << "RuntimeHostHeadlessPlugin deinitialized";
}

// ── IRuntimeHost (RuntimeMode::Headless) ──────────────────────────────────
RuntimeMode RuntimeHostHeadlessPluginObject::runtimeMode() const
{
    return RuntimeMode::Headless;
}

bool RuntimeHostHeadlessPluginObject::loadFlow(const QString& flowPath)
{
    return m_engine->loadFlow(flowPath);
}

void RuntimeHostHeadlessPluginObject::stopAllNodes()
{
    m_engine->stopAllNodes();
}

// ── INodeProvider ──────────────────────────────────────────────────────────
void RuntimeHostHeadlessPluginObject::registerNodes(QtNodes::NodeDelegateModelRegistry& registry) const
{
    // No-op: the framework itself ships no node types. Node models come from
    // dedicated providers (e.g. NodeditorNodesCore), which the core and
    // GUI engines each discover through QPluginManager::nodeProviders().
    Q_UNUSED(registry)
}

void RuntimeHostHeadlessPluginObject::registerNodesHeadless(QtNodes::NodeDelegateModelRegistry& registry) const
{
    // Same as registerNodes() — see above. HeadlessEngine gathers node types from
    // all discovered INodeProvider plugins, not from this one.
    Q_UNUSED(registry)
}

} // namespace Daqster
