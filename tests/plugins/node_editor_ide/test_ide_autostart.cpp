#include "test_ide_autostart.h"

#include "FlowUiSection.h"
#include "IdeAutoStart.h"
#include "SceneAutoStart.h"

#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/NodeDelegateModelRegistry>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <memory>

using QtNodes::DataFlowGraphModel;
using QtNodes::NodeDelegateModel;
using QtNodes::NodeDelegateModelRegistry;
using QtNodes::NodeId;

namespace {

/// One graph with the three fake model types registered.
///
/// The registry is the real one the IDE uses, so `load()` goes through the same
/// `internal-data.model-name` path the scene file does — a test that built the
/// nodes by hand would not prove the scene half.
struct TestGraph
{
    TestGraph()
        : registry(std::make_shared<NodeDelegateModelRegistry>())
    {
        registry->registerModel<IdeAutoStartSourceModel>(QStringLiteral("Test"));
        registry->registerModel<IdeAutoStartMiddleModel>(QStringLiteral("Test"));
        registry->registerModel<IdeAutoStartSinkModel>(QStringLiteral("Test"));
        graph = std::make_unique<DataFlowGraphModel>(registry);
    }

    /// One node entry in the graph-model JSON shape (`load()` reads it).
    static QJsonObject nodeJson(NodeId id, const QString& modelName)
    {
        QJsonObject internal;
        internal[QStringLiteral("model-name")] = modelName;
        internal[QStringLiteral("name")] = modelName;

        QJsonObject node;
        node[QStringLiteral("id")] = static_cast<int>(id);
        node[QStringLiteral("internal-data")] = internal;
        node[QStringLiteral("label")] = modelName;
        node[QStringLiteral("labelVisible")] = true;

        QJsonObject position;
        position[QStringLiteral("x")] = 0;
        position[QStringLiteral("y")] = 0;
        node[QStringLiteral("position")] = position;
        return node;
    }

    /// One connection entry in the graph-model JSON shape.
    static QJsonObject connectionJson(NodeId out, int outPort, NodeId in, int inPort)
    {
        QJsonObject connection;
        connection[QStringLiteral("outNodeId")] = static_cast<int>(out);
        connection[QStringLiteral("outPortIndex")] = outPort;
        connection[QStringLiteral("inNodeId")] = static_cast<int>(in);
        connection[QStringLiteral("inPortIndex")] = inPort;
        return connection;
    }

    /// The per-node `ui` entry in the v1 shape (`FlowUi::UiSection`).
    static QJsonObject uiNodeJson(bool autoStart)
    {
        QJsonObject ui;
        ui[QStringLiteral("autoStart")] = autoStart;
        ui[QStringLiteral("deembedded")] = false;
        ui[QStringLiteral("workspace")] = 0;
        return ui;
    }

    /// Build and load a scene: @p nodes in id order, @p connections between
    /// them, with @p uiNodes keyed by the node id as a string (the JSON key is
    /// what the IDE reads).
    void load(const QJsonArray& nodes,
              const QJsonObject& uiNodes,
              const QJsonArray& connections = QJsonArray{},
              bool withUi = true)
    {
        QJsonObject scene;
        scene[QStringLiteral("nodes")] = nodes;
        scene[QStringLiteral("connections")] = connections;
        if (withUi) {
            QJsonObject ui;
            ui[QStringLiteral("version")] = 1;
            ui[QStringLiteral("nodes")] = uiNodes;
            ui[QStringLiteral("workspaces")] = QJsonArray{};
            scene[QStringLiteral("ui")] = ui;
        }
        graph->load(scene);
        m_lastScene = scene;
    }

    /// The autoStart flags exactly as `loadSceneFromFile()` derives them: through
    /// the real `ui` section parser, not a hand-built map.
    QHash<NodeId, bool> autoStartFlags() const
    {
        QHash<NodeId, bool> flags;
        const QJsonObject ui = m_lastScene.value(QStringLiteral("ui")).toObject();
        if (ui.isEmpty())
            return flags;
        const FlowUi::UiSection section = FlowUi::UiSection::fromJson(ui);
        for (auto it = section.nodes.constBegin(); it != section.nodes.constEnd(); ++it)
            flags.insert(it.key(), it.value().autoStart);
        return flags;
    }

    /// What the IDE runs: the shared plan, from the graph and the scene flags.
    Daqster::SceneStartPlan plan() const
    {
        return Daqster::plannedStarts(*graph, autoStartFlags());
    }

    template<typename T>
    T* model(NodeId nodeId) const
    {
        return dynamic_cast<T*>(graph->delegateModel<NodeDelegateModel>(nodeId));
    }

    std::shared_ptr<NodeDelegateModelRegistry> registry;
    std::unique_ptr<DataFlowGraphModel> graph;

private:
    QJsonObject m_lastScene;
};

/// `source → middle → sink`, ids 0, 1, 2.
QJsonArray chainNodes()
{
    return QJsonArray{TestGraph::nodeJson(0, QStringLiteral("IdeAutoStartSource")),
                      TestGraph::nodeJson(1, QStringLiteral("IdeAutoStartMiddle")),
                      TestGraph::nodeJson(2, QStringLiteral("IdeAutoStartSink"))};
}

QJsonArray chainConnections()
{
    return QJsonArray{TestGraph::connectionJson(0, 0, 1, 0),
                      TestGraph::connectionJson(1, 0, 2, 0)};
}

} // namespace

// ── The rule, on the IDE's own graph ────────────────────────────────────────
void IdeAutoStartTest::computingThePlanStartsNothing()
{
    TestGraph scene;
    scene.load(chainNodes(), QJsonObject{}, chainConnections());

    // Exactly what `loadSceneFromFile()` does now: compute the plan, log it,
    // start nothing.
    const Daqster::SceneStartPlan plan = scene.plan();
    QCOMPARE(static_cast<int>(plan.starts.size()), 1);
    QCOMPARE(plan.starts.first().nodeId, 0u);

    QCOMPARE(scene.model<IdeAutoStartSourceModel>(0)->startCount(), 0);
    QCOMPARE(scene.model<IdeAutoStartMiddleModel>(1)->startCount(), 0);
}

void IdeAutoStartTest::noFlagStartsOnlyTheEntryPoint()
{
    TestGraph scene;
    scene.load(chainNodes(), QJsonObject{}, chainConnections());

    QSet<NodeId> running;
    const QVector<IdeAutoStart::Outcome> outcomes =
        IdeAutoStart::startPlannedNodes(*scene.graph, scene.plan(), {}, &running);

    QCOMPARE(scene.model<IdeAutoStartSourceModel>(0)->startCount(), 1);
    // The wired node is driven by its wire, not by the scene.
    QCOMPARE(scene.model<IdeAutoStartMiddleModel>(1)->startCount(), 0);
    QCOMPARE(static_cast<int>(outcomes.size()), 1);
    QCOMPARE(outcomes.first().nodeId, 0u);
    QCOMPARE(outcomes.first().action, IdeAutoStart::Outcome::Action::Started);
    QCOMPARE(static_cast<int>(running.size()), 1);
    QVERIFY(running.contains(0u));
}

void IdeAutoStartTest::falseFlagStartsNothing()
{
    TestGraph scene;
    // The only entry point of an undriven scene, explicitly forbidden.
    scene.load(QJsonArray{TestGraph::nodeJson(0, QStringLiteral("IdeAutoStartSource"))},
               QJsonObject{{QStringLiteral("0"), TestGraph::uiNodeJson(false)}});

    const Daqster::SceneStartPlan plan = scene.plan();
    QVERIFY(plan.starts.isEmpty());
    QCOMPARE(static_cast<int>(plan.suppressed.size()), 1);

    QSet<NodeId> running;
    const QVector<IdeAutoStart::Outcome> outcomes =
        IdeAutoStart::startPlannedNodes(*scene.graph, plan, {}, &running);

    QCOMPARE(scene.model<IdeAutoStartSourceModel>(0)->startCount(), 0);
    QVERIFY(outcomes.isEmpty());
    QVERIFY(running.isEmpty());
}

void IdeAutoStartTest::trueFlagForcesTheWiredNode()
{
    TestGraph scene;
    scene.load(chainNodes(),
               QJsonObject{{QStringLiteral("1"), TestGraph::uiNodeJson(true)}},
               chainConnections());

    QSet<NodeId> running;
    const QVector<IdeAutoStart::Outcome> outcomes =
        IdeAutoStart::startPlannedNodes(*scene.graph, scene.plan(), {}, &running);

    // The entry point by structure, and the middle node because the scene
    // forced it — the flag adds, it does not replace.
    QCOMPARE(scene.model<IdeAutoStartSourceModel>(0)->startCount(), 1);
    QCOMPARE(scene.model<IdeAutoStartMiddleModel>(1)->startCount(), 1);
    QCOMPARE(static_cast<int>(outcomes.size()), 2);
    QCOMPARE(outcomes.at(0).nodeId, 0u);
    QCOMPARE(outcomes.at(1).nodeId, 1u);
    QCOMPARE(static_cast<int>(running.size()), 2);
    QVERIFY(running.contains(0u));
    QVERIFY(running.contains(1u));
}

// ── Skip rules ──────────────────────────────────────────────────────────────
void IdeAutoStartTest::verdictSkipsTheOffendingNode()
{
    TestGraph scene;
    // Two undriven sources: both entry points, both startable.
    scene.load(QJsonArray{TestGraph::nodeJson(1, QStringLiteral("IdeAutoStartSource")),
                          TestGraph::nodeJson(2, QStringLiteral("IdeAutoStartSource"))},
               QJsonObject{});

    // ISceneOpening's answer: node 2 cannot run here. The other one may.
    Daqster::SceneOpenProblem problem;
    problem.nodeId = QStringLiteral("2");
    problem.nodeName = QStringLiteral("AutoStartSource");
    problem.message = QStringLiteral("липсва модел");

    QSet<NodeId> running;
    const QVector<IdeAutoStart::Outcome> outcomes = IdeAutoStart::startPlannedNodes(
        *scene.graph, scene.plan(), QVector<Daqster::SceneOpenProblem>{problem}, &running);

    QCOMPARE(scene.model<IdeAutoStartSourceModel>(1)->startCount(), 1);
    QCOMPARE(scene.model<IdeAutoStartSourceModel>(2)->startCount(), 0);
    QCOMPARE(static_cast<int>(outcomes.size()), 2);
    // The plan is ascending by id, so the order is this test's own.
    QCOMPARE(outcomes.at(0).nodeId, 1u);
    QCOMPARE(outcomes.at(0).action, IdeAutoStart::Outcome::Action::Started);
    QCOMPARE(outcomes.at(1).nodeId, 2u);
    QCOMPARE(outcomes.at(1).action, IdeAutoStart::Outcome::Action::SkippedInvalid);
}

void IdeAutoStartTest::invalidNodeIsSkipped()
{
    TestGraph scene;
    scene.load(QJsonArray{TestGraph::nodeJson(0, QStringLiteral("IdeAutoStartSource"))},
               QJsonObject{});

    auto* source = scene.model<IdeAutoStartSourceModel>(0);
    source->setInvalid(QStringLiteral("няма устройство"));

    QSet<NodeId> running;
    const QVector<IdeAutoStart::Outcome> outcomes =
        IdeAutoStart::startPlannedNodes(*scene.graph, scene.plan(), {}, &running);

    QCOMPARE(source->startCount(), 0);
    QCOMPARE(static_cast<int>(outcomes.size()), 1);
    QCOMPARE(outcomes.first().action, IdeAutoStart::Outcome::Action::SkippedInvalid);
    QCOMPARE(outcomes.first().reason, QStringLiteral("няма устройство"));
}

void IdeAutoStartTest::nonStartableFlaggedNodeIsIgnored()
{
    TestGraph scene;
    scene.load(QJsonArray{TestGraph::nodeJson(0, QStringLiteral("IdeAutoStartSink"))},
               QJsonObject{{QStringLiteral("0"), TestGraph::uiNodeJson(true)}});

    QSet<NodeId> running;
    const QVector<IdeAutoStart::Outcome> outcomes =
        IdeAutoStart::startPlannedNodes(*scene.graph, scene.plan(), {}, &running);

    // Nothing to start and nothing to complain about: a display node in a
    // scene that flagged it is a normal scene, not a defect.
    QVERIFY(outcomes.isEmpty());
    QVERIFY(running.isEmpty());
}

void IdeAutoStartTest::alreadyRunningNodeIsNotStartedTwice()
{
    TestGraph scene;
    scene.load(QJsonArray{TestGraph::nodeJson(0, QStringLiteral("IdeAutoStartSource"))},
               QJsonObject{});

    QSet<NodeId> running;
    running.insert(0); // the host's record: a previous Ctrl+R already started it

    const QVector<IdeAutoStart::Outcome> outcomes =
        IdeAutoStart::startPlannedNodes(*scene.graph, scene.plan(), {}, &running);

    QCOMPARE(scene.model<IdeAutoStartSourceModel>(0)->startCount(), 0);
    QCOMPARE(static_cast<int>(outcomes.size()), 1);
    QCOMPARE(outcomes.first().action, IdeAutoStart::Outcome::Action::AlreadyRunning);
    QCOMPARE(static_cast<int>(running.size()), 1);
}
