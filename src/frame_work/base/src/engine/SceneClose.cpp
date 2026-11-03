#include "SceneClose.h"

// Qt first: capabilities/INodeProvider.h ends with Q_DECLARE_INTERFACE, which
// needs the Qt headers to be in scope.
#include <QHash>
#include <QPair>
#include <QSet>
#include <QString>

#include "QPluginManager.h"
#include "capabilities/INodeProvider.h"
#include "capabilities/ISceneLifecycle.h"
#include "capabilities/IStoppable.h"
#include "LogCategories.h"

#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/NodeDelegateModel>

#include <algorithm>
#include <unordered_set>

namespace Daqster {

namespace {

/// Node ids of @p graph, ascending — the tie-break for every ordering decision.
QVector<QtNodes::NodeId> sortedNodeIds(const QtNodes::DataFlowGraphModel& graph)
{
    const std::unordered_set<QtNodes::NodeId> ids = graph.allNodeIds();
    QVector<QtNodes::NodeId> sorted;
    sorted.reserve(static_cast<int>(ids.size()));
    for (const QtNodes::NodeId id : ids)
        sorted.append(id);
    std::sort(sorted.begin(), sorted.end());
    return sorted;
}

/// Edges of @p graph as (producer, consumer) pairs, de-duplicated.
///
/// `allConnectionIds()` returns the connections touching a node — both its
/// inputs and its outputs — so iterating every node yields each edge twice.
/// Keeping only the entries whose input side is the node being visited counts
/// each edge exactly once.
QVector<QPair<QtNodes::NodeId, QtNodes::NodeId>> edges(const QtNodes::DataFlowGraphModel& graph,
                                                       const QVector<QtNodes::NodeId>& ids)
{
    QVector<QPair<QtNodes::NodeId, QtNodes::NodeId>> result;
    QSet<QPair<quint64, quint64>> seen;
    for (const QtNodes::NodeId nodeId : ids) {
        const std::unordered_set<QtNodes::ConnectionId> connections =
            graph.allConnectionIds(nodeId);
        for (const QtNodes::ConnectionId& connection : connections) {
            if (connection.inNodeId != nodeId)
                continue;
            const QPair<quint64, quint64> key(connection.outNodeId, connection.inNodeId);
            if (seen.contains(key))
                continue;
            seen.insert(key);
            result.append(qMakePair(connection.outNodeId, connection.inNodeId));
        }
    }
    return result;
}

} // namespace

QVector<QtNodes::NodeId> sceneStopOrder(const QtNodes::DataFlowGraphModel& graph)
{
    const QVector<QtNodes::NodeId> ids = sortedNodeIds(graph);
    const QVector<QPair<QtNodes::NodeId, QtNodes::NodeId>> connections = edges(graph, ids);

    // in-degree = how many producers feed this node. Sources have none.
    QHash<QtNodes::NodeId, int> inDegree;
    QHash<QtNodes::NodeId, QVector<QtNodes::NodeId>> consumers;
    for (const QtNodes::NodeId id : ids)
        inDegree.insert(id, 0);
    for (const auto& edge : connections) {
        if (!inDegree.contains(edge.first) || !inDegree.contains(edge.second))
            continue;
        ++inDegree[edge.second];
        consumers[edge.first].append(edge.second);
    }

    // Kahn, ties broken by ascending id (ids are already sorted, and a FIFO
    // keeps that property for the initial layer; later layers follow the
    // producer order, i.e. also ascending).
    QVector<QtNodes::NodeId> order;
    order.reserve(ids.size());
    QVector<QtNodes::NodeId> ready;
    for (const QtNodes::NodeId id : ids) {
        if (inDegree.value(id) == 0)
            ready.append(id);
    }

    QSet<QtNodes::NodeId> visited;
    while (!ready.isEmpty()) {
        const QtNodes::NodeId nodeId = ready.takeFirst();
        if (visited.contains(nodeId))
            continue;
        visited.insert(nodeId);
        order.append(nodeId);

        const QVector<QtNodes::NodeId> downstream = consumers.value(nodeId);
        for (const QtNodes::NodeId consumer : downstream) {
            const int remaining = inDegree.value(consumer) - 1;
            inDegree[consumer] = remaining;
            if (remaining == 0 && !visited.contains(consumer))
                ready.append(consumer);
        }
    }

    // A cycle cannot be ordered. Everything that is left is still stopped —
    // a close must never leave a running node behind because the graph is odd.
    for (const QtNodes::NodeId id : ids) {
        if (!visited.contains(id))
            order.append(id);
    }

    return order;
}

SceneCloseReport stopSceneNodes(QtNodes::DataFlowGraphModel& graph)
{
    SceneCloseReport report;
    report.nodes = static_cast<int>(graph.allNodeIds().size());
    if (report.nodes == 0)
        return report;

    const QVector<QtNodes::NodeId> order = sceneStopOrder(graph);
    report.stoppedIds.reserve(order.size());

    for (const QtNodes::NodeId nodeId : order) {
        auto* model = graph.delegateModel<QtNodes::NodeDelegateModel>(nodeId);
        if (model == nullptr)
            continue;

        auto* stoppable = dynamic_cast<IStoppable*>(model);
        if (stoppable == nullptr)
            continue;

        qCInfo(lcNodeEditor) << "SceneClose: stopping node:" << model->caption()
                             << "(id=" << nodeId << ")";
        stoppable->stop();
        ++report.stopped;
        report.stoppedIds.append(nodeId);
        report.stoppedNodes.append(
            QStringLiteral("%1 (id=%2)").arg(model->caption()).arg(nodeId));
    }

    return report;
}

int notifySceneClosed(const std::vector<INodeProvider*>& providers,
                      SceneCloseReason reason)
{
    int notified = 0;
    for (INodeProvider* provider : providers) {
        if (provider == nullptr)
            continue;
        auto* lifecycle = dynamic_cast<ISceneLifecycle*>(provider);
        if (lifecycle == nullptr)
            continue;   // the hook is optional
        qCInfo(lcNodeEditor) << "SceneClose: scene closed — notifying plugin"
                             << (reason == SceneCloseReason::ExplicitClose
                                     ? "explicit-close"
                                     : "replace");
        lifecycle->onSceneClosed(reason);
        ++notified;
    }
    return notified;
}

int notifySceneClosed(SceneCloseReason reason)
{
    QPluginManager* pluginManager = QPluginManager::instance();
    if (pluginManager == nullptr)
        return 0;

    std::vector<INodeProvider*> providers;
    const auto discovered = pluginManager->nodeProviders();
    providers.reserve(static_cast<size_t>(discovered.size()));
    for (INodeProvider* provider : discovered)
        providers.push_back(provider);
    return notifySceneClosed(providers, reason);
}

SceneCloseReport stopAndNotifySceneClosed(QtNodes::DataFlowGraphModel* graph,
                                          SceneCloseReason reason)
{
    SceneCloseReport report;
    report.reason = reason;
    if (graph == nullptr)
        return report;

    report = stopSceneNodes(*graph);
    report.reason = reason;
    if (report.empty())
        return report;   // nothing was open: no plugin needs a close notification

    report.notifiedProviders = notifySceneClosed(reason);
    return report;
}

SceneCloseReport stopAndNotifySceneClosed(QtNodes::DataFlowGraphModel* graph,
                                          const std::vector<INodeProvider*>& providers,
                                          SceneCloseReason reason)
{
    SceneCloseReport report;
    report.reason = reason;
    if (graph == nullptr)
        return report;

    report = stopSceneNodes(*graph);
    report.reason = reason;
    if (report.empty())
        return report;

    report.notifiedProviders = notifySceneClosed(providers, reason);
    return report;
}

SceneCloseReport closeSceneGraph(QtNodes::DataFlowGraphModel& graph,
                                 SceneCloseReason reason)
{
    SceneCloseReport report = stopAndNotifySceneClosed(&graph, reason);
    report.removed = clearGraphNodes(graph);
    return report;
}

SceneCloseReport closeSceneGraph(QtNodes::DataFlowGraphModel& graph,
                                 const std::vector<INodeProvider*>& providers,
                                 SceneCloseReason reason)
{
    SceneCloseReport report = stopAndNotifySceneClosed(&graph, providers, reason);
    report.removed = clearGraphNodes(graph);
    return report;
}

int clearGraphNodes(QtNodes::DataFlowGraphModel& graph)
{
    // Copy first: deleteNode() mutates the set being iterated.
    const QVector<QtNodes::NodeId> ids = sortedNodeIds(graph);
    for (const QtNodes::NodeId nodeId : ids)
        graph.deleteNode(nodeId);
    return ids.size();
}

} // namespace Daqster
