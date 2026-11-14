#include "NodeProblems.h"

namespace NodeProblems {

QString fallbackMessage()
{
    return QStringLiteral("невалидно състояние на възела");
}

namespace {

/// True when @p problem is the same row the user would read twice.
bool sameRow(const Daqster::SceneOpenProblem& a, const Daqster::SceneOpenProblem& b)
{
    return a.nodeId == b.nodeId && a.message == b.message;
}

void appendUnique(QVector<Daqster::SceneOpenProblem>& problems,
                  const Daqster::SceneOpenProblem& problem)
{
    for (Daqster::SceneOpenProblem& existing : problems) {
        if (!sameRow(existing, problem))
            continue;
        // Same node + same message: keep one row, but keep the richer one.
        if (existing.hint.isEmpty() && !problem.hint.isEmpty())
            existing = problem;
        return;
    }
    problems.append(problem);
}

} // namespace

QVector<Daqster::SceneOpenProblem> liveProblems(const QVector<NodeState>& nodes)
{
    QVector<Daqster::SceneOpenProblem> problems;
    problems.reserve(nodes.size());

    for (const NodeState& node : nodes) {
        if (node.state._state == QtNodes::NodeValidationState::State::Valid)
            continue;

        Daqster::SceneOpenProblem problem;
        problem.nodeId = QString::number(node.nodeId);
        problem.nodeName = node.nodeName;
        problem.message = node.state._stateMessage;
        if (problem.message.isEmpty())
            problem.message = fallbackMessage();
        problems.append(problem);
    }
    return problems;
}

QVector<Daqster::SceneOpenProblem> merge(
    const QVector<Daqster::SceneOpenProblem>& sceneProblems,
    const QVector<NodeState>& nodes)
{
    QVector<Daqster::SceneOpenProblem> merged;
    merged.reserve(sceneProblems.size() + nodes.size());

    for (const Daqster::SceneOpenProblem& problem : sceneProblems)
        appendUnique(merged, problem);
    for (const Daqster::SceneOpenProblem& problem : liveProblems(nodes))
        appendUnique(merged, problem);

    return merged;
}

bool sameProblems(const QVector<Daqster::SceneOpenProblem>& a,
                  const QVector<Daqster::SceneOpenProblem>& b)
{
    if (a.size() != b.size())
        return false;
    for (int i = 0; i < a.size(); ++i) {
        if (a.at(i).nodeId != b.at(i).nodeId || a.at(i).nodeName != b.at(i).nodeName
            || a.at(i).message != b.at(i).message || a.at(i).hint != b.at(i).hint) {
            return false;
        }
    }
    return true;
}

} // namespace NodeProblems
