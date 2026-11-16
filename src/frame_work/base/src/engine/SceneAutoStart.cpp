#include "SceneAutoStart.h"

#include "capabilities/IStartable.h"

#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/NodeDelegateModel>

#include <algorithm>

namespace Daqster {

namespace {

/// The node's model, or null when the graph dropped it (unregistered type).
QtNodes::NodeDelegateModel* modelOf(QtNodes::DataFlowGraphModel& graph,
                                    QtNodes::NodeId nodeId)
{
    if (!graph.nodeExists(nodeId))
        return nullptr;
    return graph.delegateModel<QtNodes::NodeDelegateModel>(nodeId);
}

/// True when no input port of @p model is wired — the structural half of the rule.
bool hasNoConnectedInput(QtNodes::DataFlowGraphModel& graph,
                         QtNodes::NodeId nodeId,
                         const QtNodes::NodeDelegateModel& model)
{
    const unsigned int inputs = model.nPorts(QtNodes::PortType::In);
    for (unsigned int port = 0; port < inputs; ++port) {
        if (!graph.connections(nodeId, QtNodes::PortType::In,
                               static_cast<QtNodes::PortIndex>(port)).empty())
            return false;
    }
    return true;
}

} // namespace

QVector<QtNodes::NodeId> entryPoints(QtNodes::DataFlowGraphModel& graph)
{
    QVector<QtNodes::NodeId> ids;
    for (const QtNodes::NodeId nodeId : graph.allNodeIds()) {
        auto* model = modelOf(graph, nodeId);
        if (model == nullptr)
            continue;
        if (dynamic_cast<IStartable*>(model) == nullptr)
            continue;   // a display/processing node: nothing to start
        if (!hasNoConnectedInput(graph, nodeId, *model))
            continue;   // driven by its wire, not by the scene
        ids.append(nodeId);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

SceneStartPlan plannedStarts(QtNodes::DataFlowGraphModel& graph,
                             const QHash<QtNodes::NodeId, bool>& autoStart)
{
    SceneStartPlan plan;

    const QVector<QtNodes::NodeId> entry = entryPoints(graph);
    for (const QtNodes::NodeId nodeId : entry) {
        SceneStart start;
        start.nodeId = nodeId;
        start.caption = modelOf(graph, nodeId)->caption();

        // Only an explicit `false` removes an entry point; absent means
        // "the scene said nothing", which leaves the structure in charge.
        if (autoStart.contains(nodeId) && !autoStart.value(nodeId)) {
            plan.suppressed.append(start);
            continue;
        }
        plan.starts.append(start);
    }

    // Forced starts: flagged `true` but not an entry point (it has inputs).
    QVector<QtNodes::NodeId> forced;
    for (auto it = autoStart.constBegin(); it != autoStart.constEnd(); ++it) {
        if (!it.value() || entry.contains(it.key()))
            continue;
        auto* model = modelOf(graph, it.key());
        if (model == nullptr || dynamic_cast<IStartable*>(model) == nullptr)
            continue;   // nothing to start: a flag on a display node is not an error
        forced.append(it.key());
    }
    std::sort(forced.begin(), forced.end());
    for (const QtNodes::NodeId nodeId : forced) {
        SceneStart start;
        start.nodeId = nodeId;
        start.caption = modelOf(graph, nodeId)->caption();
        start.reason = SceneStartReason::Forced;
        plan.starts.append(start);
    }

    return plan;
}

} // namespace Daqster
