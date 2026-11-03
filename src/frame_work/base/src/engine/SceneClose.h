#pragma once

#include "framework_core_export.h"

#include "capabilities/ISceneLifecycle.h"   // SceneCloseReason (REQ-SW-PL-078)

#include <QStringList>
#include <QVector>

#include <QtNodes/Definitions>

#include <vector>

namespace QtNodes {
class DataFlowGraphModel;
} // namespace QtNodes

namespace Daqster {

class INodeProvider;

/**
 * @brief What a scene close did — for logs, REST replies and tests.
 *
 * A close is not an open in reverse: opening builds, closing releases. The
 * report therefore records *what was released*, not what was built, so a host
 * can log one line instead of scattering individual messages and a test can
 * assert on the outcome without parsing logs.
 */
struct SceneCloseReport {
    /// Nodes present in the graph when the close started.
    int nodes = 0;
    /// Nodes that received `IStoppable::stop()` (models without `IStoppable` are not part).
    ///
    /// The count is per **invocation**, not per effect: `stop()` is idempotent,
    /// so a second close over a graph that was already stopped reports the same
    /// number and still changes nothing.
    int stopped = 0;
    /// Nodes removed from the graph by `closeSceneGraph()`. 0 on a no-op close.
    int removed = 0;
    /// Plugins that received `ISceneLifecycle::onSceneClosed()`.
    int notifiedProviders = 0;
    /// Why the close ran — what the plugins were told (and what they based a
    /// release/keep-warm decision on).
    SceneCloseReason reason = SceneCloseReason::Replace;
    /// Node ids in the exact order they were stopped.
    QVector<QtNodes::NodeId> stoppedIds;
    /// `<caption> (id=<n>)` per stopped node, in stop order.
    QStringList stoppedNodes;

    /// True when there was nothing to close (empty scene) — the no-op case.
    bool empty() const { return nodes == 0; }
};

/**
 * @brief Sources-first stop order for the nodes of @p graph.
 *
 * Kahn's algorithm over the graph connections: nodes with no incoming edge
 * (sources — cameras, files, microphones, keyboard) come first, consumers last.
 * Stopping a source first means the rest of the chain sees "no more data"
 * before it is stopped itself, which is what a graceful shutdown looks like;
 * stopping a sink first leaves producers pushing into a dead consumer.
 *
 * A graph with a cycle cannot be ordered: the nodes of the cycle are appended
 * in id order after the acyclic part, so a close still stops everything.
 *
 * Deterministic for the same graph: ties are broken by ascending node id.
 *
 * @param graph The graph model to order.
 * @return Every node id of the graph, exactly once.
 */
FRAMEWORK_CORE_EXPORT QVector<QtNodes::NodeId>
sceneStopOrder(const QtNodes::DataFlowGraphModel& graph);

/**
 * @brief Stop every node of @p graph through `IStoppable::stop()`.
 *
 * Uses `sceneStopOrder()` and is idempotent per node (`stop()` is specified as
 * idempotent — every host calls this from both "close scene" and "about to
 * quit"). Does **not** delete the nodes: destroying models that were never
 * stopped is exactly the defect this helper exists to prevent, and the caller
 * must still own the teardown.
 *
 * @param graph The live graph. Nodes are stopped, not touched otherwise.
 * @return What was stopped (for logging / REST / tests).
 */
FRAMEWORK_CORE_EXPORT SceneCloseReport stopSceneNodes(QtNodes::DataFlowGraphModel& graph);

/**
 * @brief Tell every plugin in @p providers that the scene is gone.
 *
 * Providers that do not implement `ISceneLifecycle` are skipped (the hook is
 * optional). Must run **after** `stopSceneNodes()` and **before** the models
 * are destroyed, so a plugin still sees its nodes' already-released state.
 *
 * @p reason says *why* the scene is going away; what the plugin keeps is its
 * own policy (a configured policy — env var, setting, UI — wins over the
 * reason). Callers that have no opinion get `Replace` — the pre-reason
 * behaviour — so an existing host keeps working unchanged.
 *
 * @param providers Node providers, typically `QPluginManager::nodeProviders()`.
 * @param reason Why the scene is going away.
 * @return How many providers were notified.
 */
FRAMEWORK_CORE_EXPORT int
notifySceneClosed(const std::vector<INodeProvider*>& providers,
                  SceneCloseReason reason = SceneCloseReason::Replace);

/**
 * @brief `notifySceneClosed()` against the process-wide plugin manager.
 * @param reason Why the scene is going away (`Replace` by default — see above).
 * @return How many providers were notified (0 when no manager exists).
 */
FRAMEWORK_CORE_EXPORT int
notifySceneClosed(SceneCloseReason reason = SceneCloseReason::Replace);

/**
 * @brief The full host-side close contract: stop, then notify plugins.
 *
 * This is the pair every runtime host must perform before it destroys a graph:
 *
 *  1. `IStoppable::stop()` on every node, sources first (`stopSceneNodes()`),
 *  2. `ISceneLifecycle::onSceneClosed()` on every node provider.
 *
 * Clearing the scene (`clearScene()` / `deleteNode()`) stays with the caller,
 * because a headless host has no graphics scene and a GUI host must let its
 * scene remove graphics objects too.
 *
 * When @p graph is null the call is a no-op returning an empty report — hosts
 * call this from "close" and from "load over a live scene" and must not have to
 * special-case a scene that was never opened.
 *
 * @param graph The live graph, or nullptr.
 * @param reason Why the scene is going away. The default is `Replace`, because
 *               this function doubles as the "stop the old scene" half of a
 *               hot-reload; an explicit close uses `closeSceneGraph()`, whose
 *               default is `ExplicitClose`.
 * @return What was stopped and how many plugins were notified.
 */
FRAMEWORK_CORE_EXPORT SceneCloseReport
stopAndNotifySceneClosed(QtNodes::DataFlowGraphModel* graph,
                         SceneCloseReason reason = SceneCloseReason::Replace);

/**
 * @brief Same as `stopAndNotifySceneClosed()` with an explicit provider list.
 * @param graph The live graph, or nullptr.
 * @param providers Providers to notify (tests pass fakes here).
 * @param reason Why the scene is going away.
 * @return What was stopped and how many plugins were notified.
 */
FRAMEWORK_CORE_EXPORT SceneCloseReport
stopAndNotifySceneClosed(QtNodes::DataFlowGraphModel* graph,
                         const std::vector<INodeProvider*>& providers,
                         SceneCloseReason reason = SceneCloseReason::Replace);

/**
 * @brief Delete every node of @p graph (widget-free `clearScene()`).
 *
 * `BasicGraphicsScene::clearScene()` does exactly this, but it needs a graphics
 * scene; a headless host has none. Node models are destroyed here, so the
 * caller must have run `stopAndNotifySceneClosed()` first.
 *
 * @param graph The graph to empty.
 * @return How many nodes were removed.
 */
FRAMEWORK_CORE_EXPORT int clearGraphNodes(QtNodes::DataFlowGraphModel& graph);

/**
 * @brief The whole close: stop, notify, free. This is what a host calls.
 *
 * `stopAndNotifySceneClosed()` + `clearGraphNodes()` — in that order, because
 * destroying a model that was never stopped is the defect the contract exists
 * to prevent. After it returns the graph is empty, so calling it again is a
 * complete no-op (`empty()`, `removed == 0`, no plugin notified): closing twice
 * cannot release a warm model twice.
 *
 * A GUI host that also owns a graphics scene (`clearScene()`) must use
 * `stopAndNotifySceneClosed()` plus its own scene teardown instead — the
 * graphics objects are its to remove, not the graph's.
 *
 * @param graph The live graph.
 * @param providers Providers to notify; the plugin-manager overload is used when
 *                  the caller has no explicit list.
 * @param reason Why the scene is going away. Defaults to `ExplicitClose` because
 *               this call *is* a close (Ctrl+W, `POST /api/flow/close`), and a
 *               plugin should be told the truth; whether it gives memory back
 *               is its own policy — the reason never implies a release. A
 *               hot-reload uses `stopAndNotifySceneClosed()`, which defaults to
 *               `Replace`.
 * @return What was stopped, notified and removed.
 */
FRAMEWORK_CORE_EXPORT SceneCloseReport
closeSceneGraph(QtNodes::DataFlowGraphModel& graph,
                SceneCloseReason reason = SceneCloseReason::ExplicitClose);
FRAMEWORK_CORE_EXPORT SceneCloseReport
closeSceneGraph(QtNodes::DataFlowGraphModel& graph,
                const std::vector<INodeProvider*>& providers,
                SceneCloseReason reason = SceneCloseReason::ExplicitClose);

} // namespace Daqster
