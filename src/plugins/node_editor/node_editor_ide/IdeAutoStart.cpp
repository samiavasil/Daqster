#include "IdeAutoStart.h"

#include "LogCategories.h"
#include "capabilities/IStartable.h"

#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/NodeDelegateModel>

#include <exception>

namespace IdeAutoStart {

namespace {

/// The ids the verdict says cannot start, parsed once: `SceneOpenProblem` keeps
/// the id as the string it had in the scene JSON.
QSet<QtNodes::NodeId> blockedNodes(const QVector<Daqster::SceneOpenProblem>& problems)
{
    QSet<QtNodes::NodeId> blocked;
    for (const Daqster::SceneOpenProblem& problem : problems) {
        if (problem.nodeId.isEmpty())
            continue;
        bool ok = false;
        const QtNodes::NodeId nodeId = problem.nodeId.toUInt(&ok);
        if (ok)
            blocked.insert(nodeId);
    }
    return blocked;
}

} // namespace

QVector<Outcome> startAutoStartNodes(QtNodes::DataFlowGraphModel& graph,
                                    const QHash<QtNodes::NodeId, bool>& autoStart,
                                    const QVector<Daqster::SceneOpenProblem>& problems,
                                    QSet<QtNodes::NodeId>* running)
{
    QVector<Outcome> outcomes;
    const QSet<QtNodes::NodeId> blocked = blockedNodes(problems);

    for (auto it = autoStart.constBegin(); it != autoStart.constEnd(); ++it) {
        const QtNodes::NodeId nodeId = it.key();

        // Absent/false means "the scene did not ask": not even a log line, or an
        // interactive user would read a skip notice for every node.
        if (!it.value())
            continue;

        // Tolerant load guard, as in the other two hosts: the graph may have
        // dropped the node (unregistered model type).
        if (!graph.nodeExists(nodeId))
            continue;

        auto* model = graph.delegateModel<QtNodes::NodeDelegateModel>(nodeId);
        if (model == nullptr)
            continue;

        auto* startable = dynamic_cast<Daqster::IStartable*>(model);
        if (startable == nullptr) {
            // A display/processing node in a scene that flagged it: not an
            // error, it simply has nothing to start.
            qCDebug(lcNodeEditor) << "IDE: няма какво да стартирам в" << model->caption();
            continue;
        }

        Outcome outcome;
        outcome.nodeId = nodeId;
        outcome.name = model->caption();

        if (running != nullptr && running->contains(nodeId)) {
            outcome.action = Outcome::Action::AlreadyRunning;
            outcomes.append(outcome);
            continue;
        }

        const QtNodes::NodeValidationState state = model->validationState();
        const bool valid = state._state == QtNodes::NodeValidationState::State::Valid;
        if (!valid || blocked.contains(nodeId)) {
            outcome.action = Outcome::Action::SkippedInvalid;
            outcome.reason = state._stateMessage;
            qCWarning(lcNodeEditor).noquote()
                << QStringLiteral("IDE: пропускам '%1' (невалиден)").arg(outcome.name);
            outcomes.append(outcome);
            continue;
        }

        try {
            qCInfo(lcNodeEditor).noquote()
                << QStringLiteral("IDE: стартирам '%1' (autoStart)").arg(outcome.name);
            startable->start();
        } catch (const std::exception& e) {
            // A start() that throws must not cost the rest of the scene.
            outcome.action = Outcome::Action::Failed;
            outcome.reason = QString::fromUtf8(e.what());
            qCWarning(lcNodeEditor).noquote()
                << QStringLiteral("IDE: '%1' не стартира:").arg(outcome.name)
                << outcome.reason;
            outcomes.append(outcome);
            continue;
        }

        outcome.action = Outcome::Action::Started;
        if (running != nullptr)
            running->insert(nodeId);
        outcomes.append(outcome);
    }

    return outcomes;
}

} // namespace IdeAutoStart
