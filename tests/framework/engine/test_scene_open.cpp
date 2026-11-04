#include <QtTest>

#include "SceneOpen.h"
#include "capabilities/INodeProvider.h"
#include "capabilities/ISceneOpening.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

#include <memory>
#include <optional>

// ═══════════════════════════════════════════════════════════════════════════
//  Scene open hook (REQ-SW-PL-082)
//
//  A scene is refused *before* `load()` — before the outgoing scene is stopped
//  and before its graph is destroyed — so a plugin can say "this will not run
//  here" at no cost. These tests drive the shared helper (FrameworkCore:
//  SceneOpen) with fake providers, so they need neither a GUI nor a plugin .so.
//
//  Limitation, also stated in the REQ: `HeadlessEngine::applySceneJson()` is
//  private and the public tree ships no test plugin, so the host-side ordering
//  (read → ask → stop → load) is not covered here. The seam — who is asked,
//  what is asked, how answers are merged — is what is tested.
// ═══════════════════════════════════════════════════════════════════════════

namespace {

/// What a fake provider saw, so a test can prove the *scene* was handed over
/// and that providers without the hook were never called.
struct ProviderLog
{
    int calls = 0;
    std::optional<QJsonObject> seenScene;
};

/// Provider that implements the optional open hook and answers with a fixed
/// list of problems (empty == accept).
class FakeOpeningProvider : public Daqster::INodeProvider,
                            public Daqster::ISceneOpening
{
public:
    explicit FakeOpeningProvider(QStringList problems,
                                 std::shared_ptr<ProviderLog> log = {})
        : m_problems(std::move(problems))
        , m_log(std::move(log))
    {
    }

    void registerNodes(QtNodes::NodeDelegateModelRegistry&) const override {}
    void registerNodesHeadless(QtNodes::NodeDelegateModelRegistry&) const override {}

    Daqster::SceneOpenVerdict onSceneOpening(const QJsonObject& scene) override
    {
        if (m_log) {
            ++m_log->calls;
            m_log->seenScene = scene;
        }
        Daqster::SceneOpenVerdict verdict;
        verdict.problems = m_problems;
        return verdict;
    }

private:
    QStringList m_problems;
    std::shared_ptr<ProviderLog> m_log;
};

/// Provider without the hook — it must simply be skipped, exactly like a
/// provider without `ISceneLifecycle` is skipped on close.
class PlainProvider : public Daqster::INodeProvider
{
public:
    void registerNodes(QtNodes::NodeDelegateModelRegistry&) const override {}
    void registerNodesHeadless(QtNodes::NodeDelegateModelRegistry&) const override {}
};

/// A minimal scene: two named nodes, so the hook can be proven to receive the
/// real object rather than an empty one.
QJsonObject scene()
{
    QJsonObject node;
    node["id"] = 1;
    node["internal-data"] = QJsonObject{{"model-name", "FakeNode"}};

    QJsonObject sceneJson;
    sceneJson["nodes"] = QJsonArray{node, node};
    sceneJson["connections"] = QJsonArray{};
    return sceneJson;
}

} // namespace

class TestSceneOpen : public QObject
{
    Q_OBJECT

private slots:
    /// A provider without the hook is optional, not an error: it is skipped and
    /// not counted among the providers that were asked.
    void providerWithoutHookIsSkipped();

    /// One provider, several reasons: the report carries **all** of them, not
    /// the first — the user should not fix one problem per attempt.
    void oneProviderReportsAllProblems();

    /// Two providers that both refuse: the report is the union of their
    /// problems, in provider order.
    void twoProvidersAreUnioned();

    /// Acceptance is an empty problem list — the report stays clean and the
    /// message is empty (nothing for a host to show).
    void acceptedSceneProducesNoProblems();

    /// No provider implements the hook at all: the scene opens, as it did
    /// before the hook existed.
    void noProvidersIsAcceptance();

    /// `providersAsked` counts the providers with the hook — asked != discovered.
    void providersAskedIsCounted();

    /// The hook receives the scene that would be loaded (the seam is handed the
    /// object, not a reconstructed summary of it).
    void hookReceivesTheScene();
};

void TestSceneOpen::providerWithoutHookIsSkipped()
{
    auto log = std::make_shared<ProviderLog>();
    FakeOpeningProvider accepting({}, log);
    PlainProvider plain;   // no hook: must be skipped without error

    std::vector<Daqster::INodeProvider*> providers{&plain, &accepting};

    const Daqster::SceneOpenReport report = Daqster::checkSceneOpening(scene(), providers);

    QVERIFY(report.accepted());
    QCOMPARE(report.providersAsked, 1);   // the plain one was not asked
    QCOMPARE(log->calls, 1);
}

void TestSceneOpen::oneProviderReportsAllProblems()
{
    FakeOpeningProvider refusing(
        {QStringLiteral("model file /models/x.gguf is missing"),
         QStringLiteral("backend 'piper' is not built in this binary"),
         QStringLiteral("node 'Chat' needs 4 GiB, budget is 2 GiB")});

    std::vector<Daqster::INodeProvider*> providers{&refusing};

    const Daqster::SceneOpenReport report = Daqster::checkSceneOpening(scene(), providers);

    QVERIFY(!report.accepted());
    QCOMPARE(report.problems.size(), 3);
    QCOMPARE(report.problems.at(0), QStringLiteral("model file /models/x.gguf is missing"));
    QCOMPARE(report.problems.at(2),
             QStringLiteral("node 'Chat' needs 4 GiB, budget is 2 GiB"));
    QCOMPARE(report.providersAsked, 1);
}

void TestSceneOpen::twoProvidersAreUnioned()
{
    FakeOpeningProvider first({QStringLiteral("first: voice file missing")});
    FakeOpeningProvider second({QStringLiteral("second: licence expired"),
                                QStringLiteral("second: gpu out of memory")});

    std::vector<Daqster::INodeProvider*> providers{&first, &second};

    const Daqster::SceneOpenReport report = Daqster::checkSceneOpening(scene(), providers);

    QVERIFY(!report.accepted());
    QCOMPARE(report.problems.size(), 3);   // nothing dropped, nothing deduplicated
    QCOMPARE(report.problems.at(0), QStringLiteral("first: voice file missing"));
    QCOMPARE(report.problems.at(1), QStringLiteral("second: licence expired"));
    QCOMPARE(report.problems.at(2), QStringLiteral("second: gpu out of memory"));
}

void TestSceneOpen::acceptedSceneProducesNoProblems()
{
    FakeOpeningProvider accepting({});
    std::vector<Daqster::INodeProvider*> providers{&accepting};

    const Daqster::SceneOpenReport report = Daqster::checkSceneOpening(scene(), providers);

    QVERIFY(report.accepted());
    QVERIFY(report.problems.isEmpty());
    QCOMPARE(report.providersAsked, 1);
    // An accepted scene has nothing to tell the user: an empty message is the
    // contract hosts rely on (no dialog, no REST error body).
    QVERIFY(Daqster::sceneOpeningMessage(report).isEmpty());
}

void TestSceneOpen::noProvidersIsAcceptance()
{
    // No plugin at all, and a provider list that contains nothing usable: both
    // are acceptance — the hook must never turn "nobody asked" into a refusal.
    const Daqster::SceneOpenReport none = Daqster::checkSceneOpening(scene(), {});
    QVERIFY(none.accepted());
    QCOMPARE(none.providersAsked, 0);

    PlainProvider plain;
    std::vector<Daqster::INodeProvider*> providers{nullptr, &plain};
    const Daqster::SceneOpenReport noHook = Daqster::checkSceneOpening(scene(), providers);
    QVERIFY(noHook.accepted());
    QCOMPARE(noHook.providersAsked, 0);
    QVERIFY(Daqster::sceneOpeningMessage(noHook).isEmpty());
}

void TestSceneOpen::providersAskedIsCounted()
{
    FakeOpeningProvider a({});
    FakeOpeningProvider b({QStringLiteral("b refuses")});
    PlainProvider plain;

    std::vector<Daqster::INodeProvider*> providers{&a, nullptr, &plain, &b};

    const Daqster::SceneOpenReport report = Daqster::checkSceneOpening(scene(), providers);

    QCOMPARE(report.providersAsked, 2);   // a and b; the null and the plain one are not
    QCOMPARE(report.problems.size(), 1);
    // The refusal message names the problem and says nothing was loaded — the
    // one text the log, the message box and the REST error body all show.
    const QString message = Daqster::sceneOpeningMessage(report);
    QVERIFY(message.contains(QStringLiteral("b refuses")));
    QVERIFY(message.contains(QStringLiteral("Nothing was loaded")));
}

void TestSceneOpen::hookReceivesTheScene()
{
    auto log = std::make_shared<ProviderLog>();
    FakeOpeningProvider accepting({}, log);
    std::vector<Daqster::INodeProvider*> providers{&accepting};

    const QJsonObject sceneJson = scene();
    Daqster::checkSceneOpening(sceneJson, providers);

    QCOMPARE(log->calls, 1);
    QVERIFY(log->seenScene.has_value());
    // The hook decides on the scene that would be loaded, so it must see its
    // nodes — not an empty object.
    QCOMPARE(log->seenScene->value("nodes").toArray().size(), 2);
}

QTEST_GUILESS_MAIN(TestSceneOpen)
#include "test_scene_open.moc"
