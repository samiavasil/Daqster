#include <QtTest>

#include "SceneClose.h"
#include "capabilities/INodeProvider.h"
#include "capabilities/ISceneLifecycle.h"
#include "capabilities/IStoppable.h"

#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/NodeData>
#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeDelegateModelRegistry>

#include <QStringList>

#include <memory>
#include <optional>

// ═══════════════════════════════════════════════════════════════════════════
//  Scene close contract (REQ-SW-PL-078)
//
//  Closing a scene is not "destroy the graph": every node must be stopped while
//  it is still alive (sources first), and every plugin must get one
//  `ISceneLifecycle::onSceneClosed()` before the models die. These tests drive
//  the shared helper (FrameworkCore: SceneClose) with fake node models, so they
//  need neither a GUI nor a plugin .so — and no real resources.
// ═══════════════════════════════════════════════════════════════════════════

namespace {

/// Shared log of the whole scenario: stop order and plugin notifications.
struct ScenarioLog
{
    QStringList stopOrder;          ///< captions, in the order stop() was called
    int         contextCount = 0;   ///< "live contexts": ++ on start, -- on stop
    int         closeNotifications = 0;
    int         contextsSeenAtClose = -1;   ///< contextCount when the hook ran
    /// What the host said when it notified — empty if the provider only
    /// implements the legacy no-argument hook (which loses the reason).
    std::optional<Daqster::SceneCloseReason> reason;
};

class FakeData : public QtNodes::NodeData
{
public:
    QtNodes::NodeDataType type() const override
    {
        return QtNodes::NodeDataType{QStringLiteral("fake"), QStringLiteral("Fake")};
    }
};

/// A node with one input and one output that records its own stop().
class FakeNode : public QtNodes::NodeDelegateModel, public Daqster::IStoppable
{
public:
    explicit FakeNode(std::shared_ptr<ScenarioLog> log)
        : m_log(std::move(log))
    {
        m_caption = QStringLiteral("fake-%1").arg(++s_counter);
    }

    QString caption() const override { return m_caption; }
    QString name() const override { return QStringLiteral("FakeNode"); }

    unsigned int nPorts(QtNodes::PortType) const override { return 1; }

    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override
    {
        return FakeData{}.type();
    }

    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override
    {
        return std::make_shared<FakeData>();
    }

    void setInData(std::shared_ptr<QtNodes::NodeData> data, QtNodes::PortIndex) override
    {
        m_in = std::move(data);
    }

    QWidget* embeddedWidget() override { return nullptr; }

    /// "A running node holds a context" (a model handle, a device, a process —
    /// the point is that it is a resource that must not outlive stop()).
    void start() { ++m_log->contextCount; }
    void stop() override
    {
        if (m_stopped)
            return;   // idempotent, like every real node
        m_stopped = true;
        m_log->stopOrder.append(m_caption);
        --m_log->contextCount;
    }

    static void resetCounter() { s_counter = 0; }

private:
    static int s_counter;
    std::shared_ptr<ScenarioLog> m_log;
    QString      m_caption;
    std::shared_ptr<QtNodes::NodeData> m_in;
    bool         m_stopped = false;
};

int FakeNode::s_counter = 0;

/// Provider that implements the optional close hook.
class FakeLifecycleProvider : public Daqster::INodeProvider,
                              public Daqster::ISceneLifecycle
{
public:
    explicit FakeLifecycleProvider(std::shared_ptr<ScenarioLog> log)
        : m_log(std::move(log))
    {
    }

    void registerNodes(QtNodes::NodeDelegateModelRegistry&) const override {}
    void registerNodesHeadless(QtNodes::NodeDelegateModelRegistry&) const override {}

    void onSceneClosed() override
    {
        ++m_log->closeNotifications;
        // The contract: this runs after the nodes were stopped, so a plugin
        // that keeps weights warm sees zero live contexts, not a half-closed
        // graph.
        m_log->contextsSeenAtClose = m_log->contextCount;
    }

private:
    std::shared_ptr<ScenarioLog> m_log;
};

/// Provider that implements the reason-aware hook (REQ-SW-PL-078): an explicit
/// close must be distinguishable from a replace, because the plugin's policy
/// differs — release the weights, or keep them warm.
class ReasonAwareProvider : public Daqster::INodeProvider,
                            public Daqster::ISceneLifecycle
{
public:
    explicit ReasonAwareProvider(std::shared_ptr<ScenarioLog> log)
        : m_log(std::move(log))
    {
    }

    void registerNodes(QtNodes::NodeDelegateModelRegistry&) const override {}
    void registerNodesHeadless(QtNodes::NodeDelegateModelRegistry&) const override {}

    void onSceneClosed() override { ++m_log->closeNotifications; }

    void onSceneClosed(Daqster::SceneCloseReason reason) override
    {
        ++m_log->closeNotifications;
        m_log->reason = reason;
    }

private:
    std::shared_ptr<ScenarioLog> m_log;
};

/// Provider without the hook — it must simply be skipped.
class PlainProvider : public Daqster::INodeProvider
{
public:
    void registerNodes(QtNodes::NodeDelegateModelRegistry&) const override {}
    void registerNodesHeadless(QtNodes::NodeDelegateModelRegistry&) const override {}
};

/// A graph with a registry that can create FakeNode instances.
class Fixture
{
public:
    Fixture()
    {
        FakeNode::resetCounter();
        m_log = std::make_shared<ScenarioLog>();
        m_registry = std::make_shared<QtNodes::NodeDelegateModelRegistry>();
        auto log = m_log;
        m_registry->registerModel<FakeNode>([log]() {
            return std::make_unique<FakeNode>(log);
        }, QStringLiteral("Test"));
        // registerModel() asks the creator for a throwaway instance to compute
        // the type name; keep the caption numbering aligned with node ids.
        FakeNode::resetCounter();
        m_graph = std::make_unique<QtNodes::DataFlowGraphModel>(m_registry);
    }

    std::shared_ptr<ScenarioLog> log() const { return m_log; }
    QtNodes::DataFlowGraphModel& graph() { return *m_graph; }

    /// Adds a node and marks it running (a live scene: every node holds a
    /// context until its stop() releases it).
    QtNodes::NodeId addNode()
    {
        const QtNodes::NodeId id = m_graph->addNode(QStringLiteral("FakeNode"));
        auto* model = m_graph->delegateModel<QtNodes::NodeDelegateModel>(id);
        if (auto* fake = dynamic_cast<FakeNode*>(model))
            fake->start();
        return id;
    }

    void connectNodes(QtNodes::NodeId out, QtNodes::NodeId in)
    {
        m_graph->addConnection({out, 0, in, 0});
    }

private:
    std::shared_ptr<ScenarioLog> m_log;
    std::shared_ptr<QtNodes::NodeDelegateModelRegistry> m_registry;
    std::unique_ptr<QtNodes::DataFlowGraphModel> m_graph;
};

} // namespace

class TestSceneClose : public QObject
{
    Q_OBJECT

private slots:
    // ── The named acceptance tests (REQ-SW-PL-078 AC2) ───────────────────

    /// Closing stops every node, sources first, and the plugin hook runs after
    /// the nodes released their contexts — but before they are destroyed.
    void sceneCloseStopsNodesAndReleasesContexts();

    /// Two closes in a row: the second one is a no-op — no second plugin
    /// notification, no double release, no error.
    void doubleCloseIsIdempotent();

    /// A close with no scene at all (never opened, or already closed) succeeds
    /// silently and notifies nobody.
    void closeWithoutSceneIsNoOp();

    // ── Ordering (the part that makes the close graceful) ────────────────

    /// Sources first: a chain built sink-first is still stopped source-first.
    void stopOrderIsSourcesFirstNotInsertionOrder();

    /// A cycle cannot be ordered but must still be stopped completely.
    void cycleIsStillFullyStopped();

    // ── The reason the scene is going away (REQ-SW-PL-078) ───────────────

    /// An explicit close tells the plugin it is a close — that is what makes
    /// "release every model" possible without a second entry point.
    void explicitCloseReasonReachesPlugin();

    /// A hot-reload tells the plugin it is a replace — the weights stay warm.
    void replaceReasonReachesPlugin();

    /// A plugin that only knows the old no-argument hook is still notified, and
    /// with a reason it can act on (the default forwards, it is never skipped).
    void legacyProviderIsStillNotifiedWithReason();
};

void TestSceneClose::sceneCloseStopsNodesAndReleasesContexts()
{
    Fixture fixture;
    const QtNodes::NodeId source = fixture.addNode();
    const QtNodes::NodeId sink = fixture.addNode();
    fixture.connectNodes(source, sink);

    auto lifecycle = std::make_unique<FakeLifecycleProvider>(fixture.log());
    PlainProvider plain;   // no hook: must be skipped without error
    std::vector<Daqster::INodeProvider*> providers{lifecycle.get(), &plain};

    QCOMPARE(fixture.log()->contextCount, 2);   // two running nodes

    const Daqster::SceneCloseReport report =
        Daqster::stopAndNotifySceneClosed(&fixture.graph(), providers);

    QCOMPARE(report.nodes, 2);
    QCOMPARE(report.stopped, 2);
    QCOMPARE(report.notifiedProviders, 1);
    // The whole point: zero live contexts — nothing was left running.
    QCOMPARE(fixture.log()->contextCount, 0);
    // ...and the plugin saw that clean state, not a half-closed graph.
    QCOMPARE(fixture.log()->closeNotifications, 1);
    QCOMPARE(fixture.log()->contextsSeenAtClose, 0);
    QCOMPARE(report.stoppedIds.size(), 2);
}

void TestSceneClose::doubleCloseIsIdempotent()
{
    Fixture fixture;
    fixture.addNode();
    fixture.addNode();

    auto lifecycle = std::make_unique<FakeLifecycleProvider>(fixture.log());
    std::vector<Daqster::INodeProvider*> providers{lifecycle.get()};

    // What a host does: stop, notify, free — one call (REQ-SW-PL-078).
    const Daqster::SceneCloseReport first =
        Daqster::closeSceneGraph(fixture.graph(), providers);
    QCOMPARE(first.stopped, 2);
    QCOMPARE(first.removed, 2);
    QCOMPARE(fixture.log()->contextCount, 0);
    QCOMPARE(fixture.log()->closeNotifications, 1);

    // The second close must change nothing at all: no node stopped again, no
    // node removed, and — the one that matters — no second plugin notification.
    // A warm model released by a close that changed nothing would lose the
    // fast reopen for nothing.
    const Daqster::SceneCloseReport second =
        Daqster::closeSceneGraph(fixture.graph(), providers);
    QVERIFY(second.empty());
    QCOMPARE(second.stopped, 0);
    QCOMPARE(second.removed, 0);
    QCOMPARE(second.notifiedProviders, 0);
    QCOMPARE(fixture.log()->stopOrder.size(), 2);          // each node stopped once
    QCOMPARE(fixture.log()->contextCount, 0);              // never went negative
    QCOMPARE(fixture.log()->closeNotifications, 1);        // plugin asked once
    QCOMPARE(static_cast<int>(fixture.graph().allNodeIds().size()), 0);
}

void TestSceneClose::closeWithoutSceneIsNoOp()
{
    auto log = std::make_shared<ScenarioLog>();
    auto lifecycle = std::make_unique<FakeLifecycleProvider>(log);
    std::vector<Daqster::INodeProvider*> providers{lifecycle.get()};

    // The host calls close from "close scene" and from "load over a live
    // scene"; a scene that was never opened must not be special-cased there.
    const Daqster::SceneCloseReport none =
        Daqster::stopAndNotifySceneClosed(nullptr, providers);
    QVERIFY(none.empty());
    QCOMPARE(none.stopped, 0);
    QCOMPARE(none.notifiedProviders, 0);

    // An existing but empty graph is the same case.
    Fixture fixture;
    const Daqster::SceneCloseReport empty =
        Daqster::stopAndNotifySceneClosed(&fixture.graph(), providers);
    QVERIFY(empty.empty());
    QCOMPARE(empty.stopped, 0);
    QCOMPARE(empty.notifiedProviders, 0);
    QCOMPARE(log->closeNotifications, 0);   // no hook invented
}

void TestSceneClose::stopOrderIsSourcesFirstNotInsertionOrder()
{
    Fixture fixture;
    // Built backwards on purpose: sink gets the lowest id, source the highest.
    // An implementation that just walks allNodeIds() would stop the sink first.
    const QtNodes::NodeId sink = fixture.addNode();
    const QtNodes::NodeId middle = fixture.addNode();
    const QtNodes::NodeId source = fixture.addNode();

    fixture.connectNodes(source, middle);
    fixture.connectNodes(middle, sink);

    const QVector<QtNodes::NodeId> order = Daqster::sceneStopOrder(fixture.graph());
    QCOMPARE(order.size(), 3);
    QCOMPARE(order.at(0), source);
    QCOMPARE(order.at(1), middle);
    QCOMPARE(order.at(2), sink);

    const Daqster::SceneCloseReport report = Daqster::stopSceneNodes(fixture.graph());
    QCOMPARE(report.stopped, 3);
    QCOMPARE(report.stoppedIds, order);
}

void TestSceneClose::cycleIsStillFullyStopped()
{
    Fixture fixture;
    const QtNodes::NodeId a = fixture.addNode();
    const QtNodes::NodeId b = fixture.addNode();
    const QtNodes::NodeId c = fixture.addNode();
    // a -> b -> c -> a : a has an incoming edge, so no source exists at all.
    fixture.connectNodes(a, b);
    fixture.connectNodes(b, c);
    fixture.connectNodes(c, a);

    const QVector<QtNodes::NodeId> order = Daqster::sceneStopOrder(fixture.graph());
    QCOMPARE(order.size(), 3);   // nothing dropped because the graph is odd

    const Daqster::SceneCloseReport report = Daqster::stopSceneNodes(fixture.graph());
    QCOMPARE(report.stopped, 3);
    QCOMPARE(fixture.log()->contextCount, 0);
}

void TestSceneClose::explicitCloseReasonReachesPlugin()
{
    Fixture fixture;
    fixture.addNode();

    auto lifecycle = std::make_unique<ReasonAwareProvider>(fixture.log());
    std::vector<Daqster::INodeProvider*> providers{lifecycle.get()};

    // `closeSceneGraph()` is the close path; its default reason is ExplicitClose.
    const Daqster::SceneCloseReport report =
        Daqster::closeSceneGraph(fixture.graph(), providers);

    QCOMPARE(report.notifiedProviders, 1);
    QCOMPARE(report.reason, Daqster::SceneCloseReason::ExplicitClose);
    QVERIFY(fixture.log()->reason.has_value());
    QCOMPARE(*fixture.log()->reason, Daqster::SceneCloseReason::ExplicitClose);
}

void TestSceneClose::replaceReasonReachesPlugin()
{
    Fixture fixture;
    fixture.addNode();

    auto lifecycle = std::make_unique<ReasonAwareProvider>(fixture.log());
    std::vector<Daqster::INodeProvider*> providers{lifecycle.get()};

    // `stopAndNotifySceneClosed()` is the "old scene goes, new one comes" half
    // of a hot-reload; its default reason is Replace.
    const Daqster::SceneCloseReport report =
        Daqster::stopAndNotifySceneClosed(&fixture.graph(), providers);

    QCOMPARE(report.notifiedProviders, 1);
    QCOMPARE(report.reason, Daqster::SceneCloseReason::Replace);
    QVERIFY(fixture.log()->reason.has_value());
    QCOMPARE(*fixture.log()->reason, Daqster::SceneCloseReason::Replace);

    // ...and the explicit reason wins over the default when one is passed.
    Fixture second;
    second.addNode();
    auto explicitProvider = std::make_unique<ReasonAwareProvider>(second.log());
    std::vector<Daqster::INodeProvider*> explicitProviders{explicitProvider.get()};
    Daqster::stopAndNotifySceneClosed(&second.graph(), explicitProviders,
                                      Daqster::SceneCloseReason::ExplicitClose);
    QVERIFY(second.log()->reason.has_value());
    QCOMPARE(*second.log()->reason, Daqster::SceneCloseReason::ExplicitClose);
}

void TestSceneClose::legacyProviderIsStillNotifiedWithReason()
{
    // Compatibility: `FakeLifecycleProvider` implements only `onSceneClosed()`.
    // The reason-aware virtual must forward to it instead of doing nothing —
    // an old plugin that never heard of `SceneCloseReason` still gets its hook,
    // and one compiled before the reason existed keeps its behaviour exactly.
    Fixture fixture;
    fixture.addNode();

    auto legacy = std::make_unique<FakeLifecycleProvider>(fixture.log());
    std::vector<Daqster::INodeProvider*> providers{legacy.get()};

    const Daqster::SceneCloseReport report = Daqster::stopAndNotifySceneClosed(
        &fixture.graph(), providers, Daqster::SceneCloseReason::ExplicitClose);

    QCOMPARE(report.notifiedProviders, 1);
    QCOMPARE(fixture.log()->closeNotifications, 1);
    QCOMPARE(fixture.log()->contextsSeenAtClose, 0);
    // The legacy hook cannot see the reason — that is exactly why the second
    // method exists, and why it must not replace the first one.
    QVERIFY(!fixture.log()->reason.has_value());
}

QTEST_GUILESS_MAIN(TestSceneClose)
#include "test_scene_close.moc"
