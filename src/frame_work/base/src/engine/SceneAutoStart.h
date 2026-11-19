#pragma once

#include "framework_core_export.h"

#include <QtNodes/Definitions>

#include <QHash>
#include <QString>
#include <QVector>

namespace QtNodes {
class DataFlowGraphModel;
} // namespace QtNodes

namespace Daqster {

/**
 * @brief Why a node is in the start plan (REQ-SW-PL-082).
 */
enum class SceneStartReason {
    EntryPoint, ///< startable and no connected input — the scene starts it by structure
    Forced      ///< `ui.nodes[].autoStart: true` — started although it has an input
};

/**
 * @brief One node a scene wants started, and why.
 */
struct SceneStart {
    QtNodes::NodeId nodeId = 0;
    QString caption;
    SceneStartReason reason = SceneStartReason::EntryPoint;
};

/**
 * @brief What a scene means by "start", computed from the graph, not from flags.
 *
 * The rule is structural (REQ-SW-PL-082): a scene starts its **entry points** —
 * the nodes that implement `IStartable` and have no connected input. A source
 * with a wire into it is driven by that wire, not by the scene, so starting it
 * twice (or at all, before upstream runs) is wrong. The `.flow` flag is an
 * override on top of that, never the rule itself:
 *
 * - `autoStart: false` — never start this node automatically (an entry point
 *   removed from the plan, or a node nothing was going to start anyway);
 * - `autoStart: true` — force-start it even though it has inputs;
 * - absent — the structure decides.
 */
struct SceneStartPlan {
    /// Nodes to start, entry points first (ascending id), then forced ones
    /// (ascending id). Deterministic: two runs of the same scene match.
    QVector<SceneStart> starts;
    /// Entry points the scene's `autoStart: false` removed. Kept so a host can
    /// say „пропускам '<name>' (autoStart:false)" instead of staying silent.
    QVector<SceneStart> suppressed;

    /// True when the scene asks for nothing at all (entry points suppressed and
    /// no forced node) — the hosts log nothing in that case.
    bool isEmpty() const { return starts.isEmpty() && suppressed.isEmpty(); }
};

/**
 * @brief The startable nodes of @p graph that have no connected input.
 *
 * A node counts only when `dynamic_cast<IStartable*>` succeeds **and** every
 * input port it declares has no connection — checked through the graph model's
 * own `connections(nodeId, PortType::In, port)`, so a graph edited in the IDE
 * and one loaded from a file answer identically.
 *
 * @param graph A loaded graph model. Nodes whose model is missing (unregistered
 *              type) are skipped; a node that throws on `nPorts()` is not — the
 *              models are expected to answer.
 * @return The entry point ids, ascending. Empty when the scene has none.
 *
 * @note Not `const`: `DataFlowGraphModel::delegateModel<T>()` is a non-const
 *       template in QtNodes, and there is no const path to the models.
 */
FRAMEWORK_CORE_EXPORT QVector<QtNodes::NodeId>
entryPoints(QtNodes::DataFlowGraphModel& graph);

/**
 * @brief `entryPoints()` with the scene's `autoStart` overrides applied.
 *
 * The one place the start rule lives: `HeadlessEngine`, `RuntimeShell` and the
 * IDE all call this and then only run their own `start()` loop, so the three
 * hosts cannot drift apart on *what* a scene means by "start".
 *
 * @param graph     The loaded graph (nodes must already exist).
 * @param autoStart Node id → the flag from the scene's `ui.nodes`. Absent means
 *                  "the scene did not ask" and leaves the structure in charge.
 * @return The starts and the suppressed entry points.
 */
FRAMEWORK_CORE_EXPORT SceneStartPlan
plannedStarts(QtNodes::DataFlowGraphModel& graph,
              const QHash<QtNodes::NodeId, bool>& autoStart);

} // namespace Daqster
