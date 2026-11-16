#include <QtTest>

#include "SceneAutoStart.h"
#include "capabilities/IStartable.h"

#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/NodeData>
#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeDelegateModelRegistry>

#include <QJsonArray>
#include <QJsonObject>

#include <memory>

// ═══════════════════════════════════════════════════════════════════════════
//  Which nodes a scene starts (REQ-SW-PL-082)
//
//  The rule is structural: an entry point is a startable node with no connected
//  input. `ui.nodes[].autoStart` is an override on top — `false` never starts a
//  node, `true` starts one that has inputs. One helper answers for the headless
//  engine, the GUI runtime and the IDE, so it is tested here once, with fakes
//  and a real DataFlowGraphModel (no plugin, no widgets, no files).
// ═══════════════════════════════════════════════════════════════════════════

namespace {

class FakeData : public QtNodes::NodeData
{
public:
    QtNodes::NodeDataType type() const override
    {
        return QtNodes::NodeDataType{QStringLiteral("fake"), QStringLiteral("Fake")};
    }
};

/// A startable source: one output, no input — an entry point by structure.
class FakeSourceNode : public QtNodes::NodeDelegateModel, public Daqster::IStartable
{
public:
    QString name() const override { return QStringLiteral("FakeSource"); }
    QString caption() const override { return QStringLiteral("source"); }

    unsigned int nPorts(QtNodes::PortType type) const override
    {
        return type == QtNodes::PortType::Out ? 1u : 0u;
    }

    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override
    {
        return FakeData{}.type();
    }

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override
    {
        return std::make_shared<FakeData>();
    }

    void setInData(std::shared_ptr<QtNodes::NodeData>, QtNodes::PortIndex) override {}

    QWidget* embeddedWidget() override { return nullptr; }

    void start() override {}
};

/// A startable middle node: one input, one output — **not** an entry point
/// until the scene forces it with `autoStart: true`.
class FakeMiddleNode : public QtNodes::NodeDelegateModel, public Daqster::IStartable
{
public:
    QString name() const override { return QStringLiteral("FakeMiddle"); }
    QString caption() const override { return QStringLiteral("middle"); }

    unsigned int nPorts(QtNodes::PortType) const override { return 1u; }

    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override
    {
        return FakeData{}.type();
    }

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override
    {
        return std::make_shared<FakeData>();
    }

    void setInData(std::shared_ptr<QtNodes::NodeData>, QtNodes::PortIndex) override {}

    QWidget* embeddedWidget() override { return nullptr; }

    void start() override {}
};

/// A sink: has an input and nothing to start, so it can never be an entry point.
class FakeSinkNode : public QtNodes::NodeDelegateModel
{
public:
    QString name() const override { return QStringLiteral("FakeSink"); }
    QString caption() const override { return QStringLiteral("sink"); }

    unsigned int nPorts(QtNodes::PortType type) const override
    {
        return type == QtNodes::PortType::In ? 1u : 0u;
    }

    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override
    {
        return FakeData{}.type();
    }

    void setInData(std::shared_ptr<QtNodes::NodeData>, QtNodes::PortIndex) override {}

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override { return nullptr; }

    QWidget* embeddedWidget() override { return nullptr; }
};

/// A loaded graph built the way a scene is: through `load()`, so the ports and
/// connections are the ones a `.flow` really produces.
class Fixture
{
public:
    Fixture()
        : m_registry(std::make_shared<QtNodes::NodeDelegateModelRegistry>())
    {
        m_registry->registerModel<FakeSourceNode>();
        m_registry->registerModel<FakeMiddleNode>();
        m_registry->registerModel<FakeSinkNode>();
        m_graph = std::make_unique<QtNodes::DataFlowGraphModel>(m_registry);
    }

    /// `source → middle → sink`, plus a second source that feeds nothing:
    /// nodes 0 and 3 are the entry points.
    void loadChain()
    {
        QJsonArray nodes;
        nodes.append(nodeJson(0, QStringLiteral("FakeSource")));
        nodes.append(nodeJson(1, QStringLiteral("FakeMiddle")));
        nodes.append(nodeJson(2, QStringLiteral("FakeSink")));
        nodes.append(nodeJson(3, QStringLiteral("FakeSource")));

        QJsonObject scene;
        scene[QStringLiteral("nodes")] = nodes;
        scene[QStringLiteral("connections")] = QJsonArray{
            connectionJson(0, 0, 1, 0),
            connectionJson(1, 0, 2, 0),
        };
        m_graph->load(scene);
    }

    QtNodes::DataFlowGraphModel& graph() { return *m_graph; }

private:
    static QJsonObject nodeJson(QtNodes::NodeId id, const QString& modelName)
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

    static QJsonObject connectionJson(QtNodes::NodeId out, int outPort,
                                      QtNodes::NodeId in, int inPort)
    {
        QJsonObject connection;
        connection[QStringLiteral("outNodeId")] = static_cast<int>(out);
        connection[QStringLiteral("outPortIndex")] = outPort;
        connection[QStringLiteral("inNodeId")] = static_cast<int>(in);
        connection[QStringLiteral("inPortIndex")] = inPort;
        return connection;
    }

    std::shared_ptr<QtNodes::NodeDelegateModelRegistry> m_registry;
    std::unique_ptr<QtNodes::DataFlowGraphModel> m_graph;
};

} // namespace

class TestSceneAutoStart : public QObject
{
    Q_OBJECT

private slots:
    /// The rule itself: `source → middle → sink` plus a second source means two
    /// entry points (the two sources) and nothing else.
    void entryPointsAreTheStartableNodesWithoutInput();

    /// No `autoStart` anywhere: a scene starts exactly its entry points — the
    /// flag is an override, not the trigger.
    void noFlagsMeansTheEntryPoints();

    /// `autoStart: false` on a source removes it; the other entry point stays.
    void falseFlagRemovesTheEntryPoint();

    /// `autoStart: true` on the middle node forces it in, after the entry
    /// points and marked as forced.
    void trueFlagForcesTheMiddleNode();

    /// A flag on a node that cannot start (a sink) changes nothing: the plan
    /// only carries nodes something can be done to.
    void flagOnNonStartableNodeIsIgnored();
};

void TestSceneAutoStart::entryPointsAreTheStartableNodesWithoutInput()
{
    Fixture scene;
    scene.loadChain();

    const QVector<QtNodes::NodeId> entry = Daqster::entryPoints(scene.graph());

    QCOMPARE(static_cast<int>(entry.size()), 2);
    QCOMPARE(entry.at(0), 0u);
    QCOMPARE(entry.at(1), 3u);
}

void TestSceneAutoStart::noFlagsMeansTheEntryPoints()
{
    Fixture scene;
    scene.loadChain();

    const Daqster::SceneStartPlan plan =
        Daqster::plannedStarts(scene.graph(), QHash<QtNodes::NodeId, bool>{});

    QCOMPARE(static_cast<int>(plan.starts.size()), 2);
    QCOMPARE(plan.starts.at(0).nodeId, 0u);
    QCOMPARE(plan.starts.at(0).reason, Daqster::SceneStartReason::EntryPoint);
    QCOMPARE(plan.starts.at(1).nodeId, 3u);
    QVERIFY(plan.suppressed.isEmpty());
}

void TestSceneAutoStart::falseFlagRemovesTheEntryPoint()
{
    Fixture scene;
    scene.loadChain();

    const Daqster::SceneStartPlan plan =
        Daqster::plannedStarts(scene.graph(), {{0, false}});

    QCOMPARE(static_cast<int>(plan.starts.size()), 1);
    QCOMPARE(plan.starts.first().nodeId, 3u);
    QCOMPARE(static_cast<int>(plan.suppressed.size()), 1);
    QCOMPARE(plan.suppressed.first().nodeId, 0u);
    QCOMPARE(plan.suppressed.first().caption, QStringLiteral("source"));
}

void TestSceneAutoStart::trueFlagForcesTheMiddleNode()
{
    Fixture scene;
    scene.loadChain();

    const Daqster::SceneStartPlan plan =
        Daqster::plannedStarts(scene.graph(), {{1, true}});

    QCOMPARE(static_cast<int>(plan.starts.size()), 3);
    QCOMPARE(plan.starts.at(2).nodeId, 1u);
    QCOMPARE(plan.starts.at(2).reason, Daqster::SceneStartReason::Forced);
    QCOMPARE(plan.starts.at(0).reason, Daqster::SceneStartReason::EntryPoint);
}

void TestSceneAutoStart::flagOnNonStartableNodeIsIgnored()
{
    Fixture scene;
    scene.loadChain();

    const Daqster::SceneStartPlan plan =
        Daqster::plannedStarts(scene.graph(), {{2, true}});

    // The sink cannot start and is not an entry point: nothing added, nothing
    // suppressed, and no error — a flagged display node is a normal scene.
    QCOMPARE(static_cast<int>(plan.starts.size()), 2);
    QVERIFY(plan.suppressed.isEmpty());
}

QTEST_MAIN(TestSceneAutoStart)
#include "test_scene_autostart.moc"
