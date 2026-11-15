#pragma once

#include "SceneOpen.h"

#include <QtNodes/Definitions>

#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

namespace QtNodes {
class DataFlowGraphModel;
} // namespace QtNodes

/**
 * @brief The IDE half of `autoStart` (REQ-SW-PL-082).
 *
 * `HeadlessEngine` and `RuntimeShell` start the nodes a scene declares with
 * `ui.nodes[].autoStart: true`; `NodeEditorIdeObject::loadSceneFromFile()` did
 * not, so a scene opened in the IDE did nothing until the user pressed ▶ on
 * every source. The loop lives here — free of the plugin object and of widgets,
 * so it can be exercised with a real `DataFlowGraphModel` and a cheap fake
 * model instead of an IDE window.
 *
 * Deliberately *not* a framework capability: the start order and the skip rules
 * are the IDE's policy (interactive users must keep the "nothing starts by
 * itself" behaviour unless the scene asks for it by name).
 */
namespace IdeAutoStart {

/// What happened to one `autoStart: true` node — for the log and for a test.
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
 * @brief Start every node @p autoStart marks, skipping the ones the verdict named.
 *
 * Iterates @p autoStart in the map's own order — the same loop the other two
 * hosts run, so all three start a scene the same way. Entries that are `false`
 * are ignored silently (a scene without `autoStart`, or with it set to `false`,
 * must behave exactly as before this existed).
 *
 * A node is skipped, with one log line, when `ISceneOpening` reported a problem
 * against it (`SceneOpenProblem::nodeId` — the JSON `ui.nodes` key as a string)
 * or when its `NodeValidationState` is not `Valid`: a node that says it cannot
 * run is not started just because it was asked to.
 *
 * Double-start guard: @p running is the host's record of what it has already
 * started. An id in it is skipped silently, and every successful `start()` is
 * recorded in it — so a reload, or a second call on the same graph, cannot start
 * a node twice. May be null when the caller keeps no record.
 *
 * @param graph     The loaded graph (the nodes must already exist).
 * @param autoStart Node id → the `autoStart` flag from the scene's `ui` section.
 * @param problems  The `ISceneOpening` verdict for this scene (may be empty).
 * @param running   Optional host-side record of started nodes.
 * @return One outcome per *startable and flagged* node, in the order visited.
 */
QVector<Outcome> startAutoStartNodes(QtNodes::DataFlowGraphModel& graph,
                                    const QHash<QtNodes::NodeId, bool>& autoStart,
                                    const QVector<Daqster::SceneOpenProblem>& problems,
                                    QSet<QtNodes::NodeId>* running);

} // namespace IdeAutoStart
