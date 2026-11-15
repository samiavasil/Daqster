#include "test_ide_autostart.h"

#include "FlowUiSection.h"
#include "IdeAutoStart.h"

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

/// One graph with the two fake model types registered.
///
/// The registry is the real one the IDE uses, so `load()` goes through the same
/// `internal-data.model-name` path the scene file does — a test that built the
/// nodes by hand would not prove the scene half.
struct TestGraph
{
    TestGraph()
        : registry(std::make_shared<NodeDelegateModelRegistry>())
    {
        registry->registerModel(
            [] { return std::make_unique<IdeAutoStartSourceModel>(); },
            QStringLiteral("Test"));
        registry->registerModel(
            [] { return std::make_unique<IdeAutoStartSinkModel>(); },
            QStringLiteral("Test"));
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

    /// The per-node `ui` entry in the v1 shape (`FlowUi::UiSection`).
    static QJsonObject uiNodeJson(bool autoStart)
    {
        QJsonObject ui;
        ui[QStringLiteral("autoStart")] = autoStart;
        ui[QStringLiteral("deembedded")] = false;
        ui[QStringLiteral("workspace")] = 0;
        return ui;
    }

    /// Build and load a scene: @p models in id order, with @p uiNodes keyed by
    /// the node id as a string (the JSON key is what the IDE reads).
    void load(const QJsonArray& nodes, const QJsonObject& uiNodes, bool withUi = true)
    {
        QJsonObject scene;
        scene[QStringLiteral("nodes")] = nodes;
        scene[QStringLiteral("connections")] = QJsonArray{};
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

} // namespace

// ── The acceptance case ─────────────────────────────────────────────────────
void IdeAutoStartTest::onlyTheFlaggedNodeStarts()
{
    TestGraph scene;
    scene.load(QJsonArray{TestGraph::nodeJson(0, QStringLiteral("IdeAutoStartSource")),
                          TestGraph::nodeJson(1, QStringLiteral("IdeAutoStartSource"))},
               QJsonObject{{QStringLiteral("0"), TestGraph::uiNodeJson(true)},
                           {QStringLiteral("1"), TestGraph::uiNodeJson(false)}});

    auto* flagged = scene.model<IdeAutoStartSourceModel>(0);
    auto* plain = scene.model<IdeAutoStartSourceModel>(1);
    QVERIFY(flagged != nullptr);
    QVERIFY(plain != nullptr);

    QSet<NodeId> running;
    const QVector<IdeAutoStart::Outcome> outcomes =
        IdeAutoStart::startAutoStartNodes(*scene.graph, scene.autoStartFlags(), {}, &running);

    // Exactly one node is running, and it is the flagged one.
    QCOMPARE(flagged->startCount(), 1);
    QCOMPARE(plain->startCount(), 0);
    QCOMPARE(outcomes.size(), 1);
    QCOMPARE(outcomes.first().nodeId, 0u);
    QCOMPARE(outcomes.first().action, IdeAutoStart::Outcome::Action::Started);
    QCOMPARE(running.size(), 1);
    QVERIFY(running.contains(0u));
}

void IdeAutoStartTest::absentFlagStartsNothing()
{
    TestGraph scene;
    // No "ui" section at all: an old scene, or a scene nobody flagged.
    scene.load(QJsonArray{TestGraph::nodeJson(0, QStringLiteral("IdeAutoStartSource"))},
               QJsonObject{}, /*withUi=*/false);

    QSet<NodeId> running;
    const QVector<IdeAutoStart::Outcome> outcomes =
        IdeAutoStart::startAutoStartNodes(*scene.graph, scene.autoStartFlags(), {}, &running);

    QCOMPARE(scene.model<IdeAutoStartSourceModel>(0)->startCount(), 0);
    QVERIFY(outcomes.isEmpty());
    QVERIFY(running.isEmpty());
}

void IdeAutoStartTest::falseFlagStartsNothing()
{
    TestGraph scene;
    scene.load(QJsonArray{TestGraph::nodeJson(0, QStringLiteral("IdeAutoStartSource"))},
               QJsonObject{{QStringLiteral("0"), TestGraph::uiNodeJson(false)}});

    QSet<NodeId> running;
    IdeAutoStart::startAutoStartNodes(*scene.graph, scene.autoStartFlags(), {}, &running);

    QCOMPARE(scene.model<IdeAutoStartSourceModel>(0)->startCount(), 0);
    QVERIFY(running.isEmpty());
}

// ── Skip rules ──────────────────────────────────────────────────────────────
void IdeAutoStartTest::verdictSkipsTheOffendingNode()
{
    TestGraph scene;
    scene.load(QJsonArray{TestGraph::nodeJson(0, QStringLiteral("IdeAutoStartSource")),
                          TestGraph::nodeJson(1, QStringLiteral("IdeAutoStartSource"))},
               QJsonObject{{QStringLiteral("0"), TestGraph::uiNodeJson(true)},
                           {QStringLiteral("1"), TestGraph::uiNodeJson(true)}});

    // ISceneOpening's answer: node 1 cannot run here. The other one may.
    Daqster::SceneOpenProblem problem;
    problem.nodeId = QStringLiteral("1");
    problem.nodeName = QStringLiteral("AutoStartSource");
    problem.message = QStringLiteral("липсва модел");

    QSet<NodeId> running;
    const QVector<IdeAutoStart::Outcome> outcomes = IdeAutoStart::startAutoStartNodes(
        *scene.graph, scene.autoStartFlags(), QVector<Daqster::SceneOpenProblem>{problem},
        &running);

    QCOMPARE(scene.model<IdeAutoStartSourceModel>(0)->startCount(), 1);
    QCOMPARE(scene.model<IdeAutoStartSourceModel>(1)->startCount(), 0);
    QCOMPARE(outcomes.size(), 2);
    // The map's iteration order is not the scene's order (QHash), so the
    // outcomes are matched by id — the *actions* are what the test is about.
    for (const IdeAutoStart::Outcome& outcome : outcomes) {
        if (outcome.nodeId == 0u)
            QCOMPARE(outcome.action, IdeAutoStart::Outcome::Action::Started);
        else if (outcome.nodeId == 1u)
            QCOMPARE(outcome.action, IdeAutoStart::Outcome::Action::SkippedInvalid);
        else
            QFAIL("unexpected node id in outcomes");
    }
}

void IdeAutoStartTest::invalidNodeIsSkipped()
{
    TestGraph scene;
    scene.load(QJsonArray{TestGraph::nodeJson(0, QStringLiteral("IdeAutoStartSource"))},
               QJsonObject{{QStringLiteral("0"), TestGraph::uiNodeJson(true)}});

    auto* source = scene.model<IdeAutoStartSourceModel>(0);
    source->setInvalid(QStringLiteral("няма устройство"));

    QSet<NodeId> running;
    const QVector<IdeAutoStart::Outcome> outcomes =
        IdeAutoStart::startAutoStartNodes(*scene.graph, scene.autoStartFlags(), {}, &running);

    QCOMPARE(source->startCount(), 0);
    QCOMPARE(outcomes.size(), 1);
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
        IdeAutoStart::startAutoStartNodes(*scene.graph, scene.autoStartFlags(), {}, &running);

    // Nothing to start and nothing to complain about: a display node in a
    // scene that flagged it is a normal scene, not a defect.
    QVERIFY(outcomes.isEmpty());
    QVERIFY(running.isEmpty());
}

void IdeAutoStartTest::alreadyRunningNodeIsNotStartedTwice()
{
    TestGraph scene;
    scene.load(QJsonArray{TestGraph::nodeJson(0, QStringLiteral("IdeAutoStartSource"))},
               QJsonObject{{QStringLiteral("0"), TestGraph::uiNodeJson(true)}});

    QSet<NodeId> running;
    running.insert(0); // the host's record: it is already running

    const QVector<IdeAutoStart::Outcome> outcomes =
        IdeAutoStart::startAutoStartNodes(*scene.graph, scene.autoStartFlags(), {}, &running);

    QCOMPARE(scene.model<IdeAutoStartSourceModel>(0)->startCount(), 0);
    QCOMPARE(outcomes.size(), 1);
    QCOMPARE(outcomes.first().action, IdeAutoStart::Outcome::Action::AlreadyRunning);
    QCOMPARE(running.size(), 1);
}
