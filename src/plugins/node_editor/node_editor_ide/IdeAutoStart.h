#pragma once

#include "SceneAutoStart.h"
#include "SceneOpen.h"

#include <QtNodes/Definitions>

#include <QSet>
#include <QString>
#include <QVector>

namespace QtNodes {
class DataFlowGraphModel;
} // namespace QtNodes

/**
 * @brief The IDE half of the scene start (REQ-SW-PL-082).
 *
 * *What* a scene starts is structural and owned by `Daqster::plannedStarts()`
 * (the entry points of the graph, with `ui.nodes[].autoStart` as an override) —
 * the same answer `HeadlessEngine` and `RuntimeShell` act on. *How* it starts
 * them is here: the IDE's skip rules, its log lines and its double-start guard,
 * free of the plugin object and of widgets, so it can be exercised with a real
 * `DataFlowGraphModel` and a cheap fake model instead of an IDE window.
 *
 * The IDE does **not** call this on open: opening a scene starts nothing. The
 * user's „Пусни сцената" (`Ctrl+R`) does.
 */
namespace IdeAutoStart {

/// What happened to one node of the plan — for the log and for a test.
struct Outcome
{
    enum class Action {
        Started,           ///< `IStartable::start()` was called
        SkippedInvalid,    ///< the open verdict or the node's own state says it cannot run
        AlreadyRunning,    ///< the host already started it — no second `start()`
        Failed             ///< `start()` threw; the node was not counted as running
    };

    QtNodes::NodeId nodeId = 0;
    QString name;
    Action action = Action::Started;
    QString reason;
};

/**
 * @brief Start the nodes @p plan lists, skipping the ones the verdict named.
 *
 * Iterates `plan.starts` in its own order (entry points, then forced ones), so
 * the three hosts start a scene the same way. `plan.suppressed` is logged here
 * too — one line per entry point the scene's `autoStart: false` removed.
 *
 * A node is skipped, with one log line, when `ISceneOpening` reported a problem
 * against it (`SceneOpenProblem::nodeId` — the JSON `ui.nodes` key as a string)
 * or when its `NodeValidationState` is not `Valid`: a node that says it cannot
 * run is not started just because the scene asks for it.
 *
 * Double-start guard: @p running is the host's record of what it has already
 * started. An id in it is skipped silently, and every successful `start()` is
 * recorded in it — so a second „Пусни сцената", or a run after a run, cannot
 * start a node twice. May be null when the caller keeps no record.
 *
 * @param graph    The loaded graph (the nodes must already exist).
 * @param plan     The result of `Daqster::plannedStarts()` for this scene.
 * @param problems The `ISceneOpening` verdict for this scene (may be empty).
 * @param running  Optional host-side record of started nodes.
 * @return One outcome per *planned* node, in the order visited.
 */
QVector<Outcome> startPlannedNodes(QtNodes::DataFlowGraphModel& graph,
                                   const Daqster::SceneStartPlan& plan,
                                   const QVector<Daqster::SceneOpenProblem>& problems,
                                   QSet<QtNodes::NodeId>* running);

} // namespace IdeAutoStart
