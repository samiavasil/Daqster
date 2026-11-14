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
    /// Path handed to the two-argument overload; empty means "no file", which is
    /// what an in-memory scene must produce (REQ-SW-PL-082).
    std::optional<QString> seenPath;
};

/// Provider that implements the optional open hook and answers with a fixed
/// list of problems (empty == accept).
class FakeOpeningProvider : public Daqster::INodeProvider,
                            public Daqster::ISceneOpening
{
public:
    explicit FakeOpeningProvider(QVector<Daqster::SceneOpenProblem> problems,
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
    QVector<Daqster::SceneOpenProblem> m_problems;
    std::shared_ptr<ProviderLog> m_log;
};

/// Provider that overrides the **path-carrying** overload — the shape a plugin
/// uses when it needs a `project.json` next to the scene.
class FakePathAwareProvider : public Daqster::INodeProvider,
                              public Daqster::ISceneOpening
{
public:
    explicit FakePathAwareProvider(std::shared_ptr<ProviderLog> log)
        : m_log(std::move(log))
    {
    }

    void registerNodes(QtNodes::NodeDelegateModelRegistry&) const override {}
    void registerNodesHeadless(QtNodes::NodeDelegateModelRegistry&) const override {}

    // The single-argument form is pure virtual, so a path-aware plugin still
    // has to provide it; hosts are expected to call the two-argument one.
    Daqster::SceneOpenVerdict onSceneOpening(const QJsonObject&) override
    {
        return {};
    }

    Daqster::SceneOpenVerdict onSceneOpening(const QJsonObject& scene,
                                             const QString& scenePath) override
    {
        if (m_log) {
            ++m_log->calls;
            m_log->seenScene = scene;
            m_log->seenPath = scenePath;
        }
        return {};
    }

private:
    std::shared_ptr<ProviderLog> m_log;
};

/// One structured problem, addressed to a node.
Daqster::SceneOpenProblem problem(const QString& nodeId, const QString& nodeName,
                                  const QString& message, const QString& hint = {})
{
    Daqster::SceneOpenProblem entry;
    entry.nodeId = nodeId;
    entry.nodeName = nodeName;
    entry.message = message;
    entry.hint = hint;
    return entry;
}

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

    /// A plugin that overrides the path-carrying overload really gets the file
    /// the scene came from — that is how it finds a `project.json` next to it.
    void scenePathReachesTheHook();

    /// A plugin built against the single-argument interface is not broken: the
    /// default implementation forwards, and the scene still arrives.
    void oldProviderStillWorks();

    /// An in-memory scene has no file: the hook gets an empty path, never a
    /// fabricated one.
    void emptyPathIsAllowed();

    /// `problemsJson()` exposes id + name + message + hint so a host can mark
    /// the offending node instead of parsing prose.
    void problemsAreSerialised();

    /// The rendered line addresses the node: `<nodeName> (<nodeId>): <message>
    /// — <hint>`, with empty parts skipped.
    void messageNamesNodeAndHint();
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
        {problem(QStringLiteral("1"), QStringLiteral("Chat"),
                 QStringLiteral("model file /models/x.gguf is missing"),
                 QStringLiteral("pick an existing .gguf in the node settings")),
         problem(QStringLiteral("2"), QStringLiteral("Tts"),
                 QStringLiteral("backend 'piper' is not built in this binary")),
         problem(QString(), QStringLiteral("scene"),
                 QStringLiteral("node 'Chat' needs 4 GiB, budget is 2 GiB"))});

    std::vector<Daqster::INodeProvider*> providers{&refusing};

    const Daqster::SceneOpenReport report = Daqster::checkSceneOpening(scene(), providers);

    QVERIFY(!report.accepted());
    QCOMPARE(report.problems.size(), 3);
    QCOMPARE(report.problems.at(0).nodeId, QStringLiteral("1"));
    QCOMPARE(report.problems.at(0).nodeName, QStringLiteral("Chat"));
    QCOMPARE(report.problems.at(0).message,
             QStringLiteral("model file /models/x.gguf is missing"));
    QCOMPARE(report.problems.at(0).hint,
             QStringLiteral("pick an existing .gguf in the node settings"));
    QCOMPARE(report.problems.at(2).message,
             QStringLiteral("node 'Chat' needs 4 GiB, budget is 2 GiB"));
    QCOMPARE(report.providersAsked, 1);
}

void TestSceneOpen::twoProvidersAreUnioned()
{
    FakeOpeningProvider first({problem(QStringLiteral("1"), QStringLiteral("Asr"),
                                       QStringLiteral("first: voice file missing"))});
    FakeOpeningProvider second({problem(QStringLiteral("2"), QStringLiteral("Chat"),
                                        QStringLiteral("second: licence expired")),
                                problem(QStringLiteral("3"), QStringLiteral("Vlm"),
                                        QStringLiteral("second: gpu out of memory"))});

    std::vector<Daqster::INodeProvider*> providers{&first, &second};

    const Daqster::SceneOpenReport report = Daqster::checkSceneOpening(scene(), providers);

    QVERIFY(!report.accepted());
    QCOMPARE(report.problems.size(), 3);   // nothing dropped, nothing deduplicated
    QCOMPARE(report.problems.at(0).message, QStringLiteral("first: voice file missing"));
    QCOMPARE(report.problems.at(1).message, QStringLiteral("second: licence expired"));
    QCOMPARE(report.problems.at(2).message, QStringLiteral("second: gpu out of memory"));
    QCOMPARE(report.problems.at(2).nodeId, QStringLiteral("3"));
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
    const Daqster::SceneOpenReport none =
        Daqster::checkSceneOpening(scene(), std::vector<Daqster::INodeProvider*>{});
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
    FakeOpeningProvider b({problem(QStringLiteral("7"), QStringLiteral("Chat"),
                                   QStringLiteral("b refuses"))});
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

void TestSceneOpen::problemsAreSerialised()
{
    FakeOpeningProvider refusing(
        {problem(QStringLiteral("4"), QStringLiteral("Chat"),
                 QStringLiteral("model file is missing"),
                 QStringLiteral("pick an existing .gguf")),
         problem(QString(), QStringLiteral("scene"),
                 QStringLiteral("settings entry 'runtime.closePolicy' is invalid"))});

    std::vector<Daqster::INodeProvider*> providers{&refusing};
    const Daqster::SceneOpenReport report = Daqster::checkSceneOpening(scene(), providers);

    const QJsonArray json = report.problemsJson();
    QCOMPARE(json.size(), 2);

    const QJsonObject first = json.at(0).toObject();
    QCOMPARE(first.value(QStringLiteral("nodeId")).toString(), QStringLiteral("4"));
    QCOMPARE(first.value(QStringLiteral("nodeName")).toString(), QStringLiteral("Chat"));
    QCOMPARE(first.value(QStringLiteral("message")).toString(),
             QStringLiteral("model file is missing"));
    QCOMPARE(first.value(QStringLiteral("hint")).toString(),
             QStringLiteral("pick an existing .gguf"));

    // A problem about the scene as a whole has no node id: the field is there
    // and empty, so a consumer can rely on all four keys.
    const QJsonObject second = json.at(1).toObject();
    QVERIFY(second.contains(QStringLiteral("nodeId")));
    QVERIFY(second.value(QStringLiteral("nodeId")).toString().isEmpty());
    QCOMPARE(second.size(), 4);

    // An accepted report serialises to an empty array, not to null.
    const QJsonArray none = Daqster::SceneOpenReport{}.problemsJson();
    QVERIFY(none.isEmpty());
}

void TestSceneOpen::messageNamesNodeAndHint()
{
    FakeOpeningProvider refusing(
        {problem(QStringLiteral("1"), QStringLiteral("Chat"),
                 QStringLiteral("model file is missing"),
                 QStringLiteral("pick an existing .gguf")),
         // No hint: the line must not end with a dangling dash.
         problem(QStringLiteral("2"), QStringLiteral("Tts"),
                 QStringLiteral("backend is not built")),
         // No node at all: the line is just the message.
         problem(QString(), QString(), QStringLiteral("no settings store"))});

    std::vector<Daqster::INodeProvider*> providers{&refusing};
    const Daqster::SceneOpenReport report = Daqster::checkSceneOpening(scene(), providers);

    const QString message = Daqster::sceneOpeningMessage(report);
    QVERIFY(message.contains(QStringLiteral("Chat (1): model file is missing — pick an existing .gguf")));
    QVERIFY(message.contains(QStringLiteral("Tts (2): backend is not built")));
    QVERIFY(message.contains(QStringLiteral("no settings store")));
    QVERIFY(!message.contains(QStringLiteral("backend is not built —")));
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

void TestSceneOpen::scenePathReachesTheHook()
{
    auto log = std::make_shared<ProviderLog>();
    FakePathAwareProvider provider(log);
    std::vector<Daqster::INodeProvider*> providers{&provider};

    const QString path = QStringLiteral("/scenes/demo/project.flow");
    Daqster::checkSceneOpening(scene(), path, providers);

    QCOMPARE(log->calls, 1);
    QCOMPARE(log->seenPath.value_or(QStringLiteral("<none>")), path);
    QVERIFY(log->seenScene.has_value());
    QCOMPARE(log->seenScene->value("nodes").toArray().size(), 2);
}

void TestSceneOpen::oldProviderStillWorks()
{
    // FakeOpeningProvider implements only `onSceneOpening(scene)` — the old
    // interface. The default of the new overload must forward to it.
    auto log = std::make_shared<ProviderLog>();
    FakeOpeningProvider accepting({}, log);
    std::vector<Daqster::INodeProvider*> providers{&accepting};

    const Daqster::SceneOpenReport report =
        Daqster::checkSceneOpening(scene(), QStringLiteral("/scenes/old.flow"), providers);

    QVERIFY(report.accepted());
    QCOMPARE(report.providersAsked, 1);
    QCOMPARE(log->calls, 1);
    QVERIFY(log->seenScene.has_value());
    // The legacy provider never sees a path: the default swallowed it and
    // answered from the scene alone.
    QVERIFY(!log->seenPath.has_value());
}

void TestSceneOpen::emptyPathIsAllowed()
{
    auto log = std::make_shared<ProviderLog>();
    FakePathAwareProvider provider(log);
    std::vector<Daqster::INodeProvider*> providers{&provider};

    // The legacy overload (no path) is still the one an in-memory caller uses.
    const Daqster::SceneOpenReport report = Daqster::checkSceneOpening(scene(), providers);

    QVERIFY(report.accepted());
    QCOMPARE(log->calls, 1);
    QVERIFY(log->seenPath.has_value());
    QVERIFY(log->seenPath->isEmpty());
}

QTEST_GUILESS_MAIN(TestSceneOpen)
#include "test_scene_open.moc"
