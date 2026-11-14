#pragma once

#include "SceneOpen.h"

#include <QtNodes/Definitions>
#include <QtNodes/NodeDelegateModel>

#include <QString>
#include <QVector>

/**
 * @brief Live node validation errors in the problems panel (REQ-SW-PL-082).
 *
 * The panel used to show only what `ISceneOpening` said *at load time*. A node
 * that turns red later (a missing backend chosen in its widget, a model file
 * that vanished) painted `NodeValidationState::Error`, but the message stayed a
 * tooltip on the canvas: "red but no text". These helpers turn the graph's
 * validation states into panel rows and merge them with the open-time verdict.
 *
 * Deliberately free of graph-model and plugin-object dependencies: the merge
 * rules (which are what the user sees) are testable with plain data, and the
 * host only supplies the states it read from the graph.
 */
namespace NodeProblems {

/// One node's validation state, detached from the graph model so it can be
/// merged and asserted on without building a `DataFlowGraphModel`.
struct NodeState
{
    QtNodes::NodeId nodeId = 0;
    /// Caption or title shown to the user (`NodeRole::Caption`).
    QString nodeName;
    QtNodes::NodeValidationState state;
};

/// Never empty: a node that is not `Valid` but carries no message still gets a
/// row, otherwise the user sees a red node and no explanation anywhere.
QString fallbackMessage();

/// One panel problem per node whose state is not `Valid`, in the input order.
/// The hint stays empty — a validation state has no remedy text, and inventing
/// one would put words in the node's mouth.
QVector<Daqster::SceneOpenProblem> liveProblems(const QVector<NodeState>& nodes);

/// The open-time verdict plus the live node errors, de-duplicated by
/// (`nodeId`, `message`): a node marked by `applySceneProblems()` reports the
/// same text as a live state, and the user must see it once. Scene-wide
/// problems (empty `nodeId`) are kept as they are, in their original order.
/// When both copies exist, the one with a hint survives (the verdict is richer).
QVector<Daqster::SceneOpenProblem> merge(
    const QVector<Daqster::SceneOpenProblem>& sceneProblems,
    const QVector<NodeState>& nodes);

/// True when two problem lists would render identically — lets the host skip a
/// panel rebuild on every `nodeUpdated` a per-frame node emits.
bool sameProblems(const QVector<Daqster::SceneOpenProblem>& a,
                  const QVector<Daqster::SceneOpenProblem>& b);

} // namespace NodeProblems
