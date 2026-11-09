#include "test_scene_problems_panel.h"

#include "SceneProblemsPanel.h"

#include <QApplication>
#include <QKeyEvent>
#include <QListWidget>
#include <QSignalSpy>
#include <QStringList>
#include <QToolButton>

// REQ-SW-PL-082. The panel is the user-facing half of the verdict: it must say
// *which* node is broken and *what to do*, and it must stay out of the way when
// there is nothing to say.

namespace {

Daqster::SceneOpenProblem makeProblem(const QString& nodeId, const QString& nodeName,
                                      const QString& message, const QString& hint)
{
    Daqster::SceneOpenProblem problem;
    problem.nodeId = nodeId;
    problem.nodeName = nodeName;
    problem.message = message;
    problem.hint = hint;
    return problem;
}

} // namespace

void TestSceneProblemsPanel::emptyProblemsHidePanel()
{
    QWidget host;
    SceneProblemsPanel panel(&host);

    panel.setProblems({makeProblem(QStringLiteral("3"), QStringLiteral("Chat"),
                                   QStringLiteral("моделът липсва"),
                                   QStringLiteral("изтеглете го"))});
    QVERIFY2(!panel.isHidden(), "a problem must be shown, not silently dropped");

    panel.setProblems({});

    QVERIFY2(panel.isHidden(), "a clean scene must not reserve canvas space");
    QCOMPARE(panel.problemCount(), 0);
    QVERIFY(panel.nodeIds().isEmpty());
    QVERIFY(panel.headerText().isEmpty());
}

void TestSceneProblemsPanel::problemsFillListAndStoreIds()
{
    QWidget host;
    SceneProblemsPanel panel(&host);

    const QVector<Daqster::SceneOpenProblem> problems{
        makeProblem(QStringLiteral("3"), QStringLiteral("Chat"),
                    QStringLiteral("моделът липсва"), QStringLiteral("изтеглете го")),
        makeProblem(QString(), QStringLiteral("сцена"),
                    QStringLiteral("няма ui секция"), QString())};

    panel.setProblems(problems);

    QCOMPARE(panel.problemCount(), 2);
    QCOMPARE(panel.nodeIds(),
             QStringList({QStringLiteral("3"), QString()}));
    QVERIFY2(panel.listWidget() != nullptr, "the list is the part the user interacts with");
    QCOMPARE(panel.listWidget()->count(), 2);
    QVERIFY2(panel.headerText().contains(QStringLiteral("2")),
             "the header must name how many problems there are");

    // Every message is visible in full: all problems at once, not the first.
    QVERIFY(panel.listWidget()->item(0)->text().contains(QStringLiteral("моделът липсва")));
    QVERIFY(panel.listWidget()->item(1)->text().contains(QStringLiteral("ui")));
}

void TestSceneProblemsPanel::activationEmitsNodeId()
{
    QWidget host;
    SceneProblemsPanel panel(&host);
    panel.setProblems({
        makeProblem(QStringLiteral("7"), QStringLiteral("Tts"),
                    QStringLiteral("backend липсва"), QStringLiteral("сменете го")),
        makeProblem(QStringLiteral("9"), QStringLiteral("Asr"),
                    QStringLiteral("моделът липсва"), QString()),
    });

    QSignalSpy spy(&panel, &SceneProblemsPanel::problemActivated);

    panel.activateRow(1);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("9"));

    // Out-of-range rows are a no-op, never a crash or a stray empty signal.
    panel.activateRow(5);
    panel.activateRow(-1);
    QCOMPARE(spy.count(), 1);
}

void TestSceneProblemsPanel::problemTextIncludesHint()
{
    const Daqster::SceneOpenProblem problem =
        makeProblem(QStringLiteral("4"), QStringLiteral("Chat"),
                    QStringLiteral("моделът липсва"),
                    QStringLiteral("изтеглете gguf файла"));

    const QString text = SceneProblemsPanel::textForProblem(problem);

    QVERIFY(text.contains(QStringLiteral("Chat (4)")));
    QVERIFY(text.contains(QStringLiteral("моделът липсва")));
    QVERIFY2(text.contains(QStringLiteral("—")) && text.contains(QStringLiteral("изтеглете gguf файла")),
             "the hint is the actionable half — it must reach the row");

    // A problem about the whole scene has no node to name, but still reads well.
    const QString sceneWide = SceneProblemsPanel::textForProblem(
        makeProblem(QString(), QString(), QStringLiteral("няма nodes"), QString()));
    QCOMPARE(sceneWide, QStringLiteral("няма nodes"));
}

void TestSceneProblemsPanel::enterKeyActivatesSelectedRow()
{
    QWidget host;
    SceneProblemsPanel panel(&host);
    panel.setProblems({
        makeProblem(QStringLiteral("11"), QStringLiteral("Vlm"),
                    QStringLiteral("-mmproj липсва"), QStringLiteral("посочете го")),
    });

    QSignalSpy spy(&panel, &SceneProblemsPanel::problemActivated);
    panel.listWidget()->setCurrentRow(0);

    QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(panel.listWidget(), &enter);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("11"));
}

void TestSceneProblemsPanel::panelStartsCollapsedAndExpands()
{
    QWidget host;
    SceneProblemsPanel panel(&host);

    QVERIFY2(panel.isCollapsed(), "the list is folded away until asked for");
    QVERIFY2(panel.isHidden(), "and the whole panel is hidden while empty");

    panel.setProblems({makeProblem(QStringLiteral("1"), QStringLiteral("N"),
                                   QStringLiteral("m"), QString())});
    QVERIFY2(panel.isCollapsed(), "a new verdict opens the header, not the list");
    QVERIFY(!panel.isHidden());

    panel.setCollapsed(false);
    QVERIFY(!panel.isCollapsed());
    QVERIFY(!panel.listWidget()->isHidden());

    panel.setCollapsed(true);
    QVERIFY(panel.isCollapsed());
}

// ── "Поправи" (REQ-SW-PL-085) ───────────────────────────────────────────────

void TestSceneProblemsPanel::problemWithoutFixerHasNoButton()
{
    QWidget host;
    SceneProblemsPanel panel(&host);
    panel.setProblems({makeProblem(QStringLiteral("3"), QStringLiteral("Chat"),
                                   QStringLiteral("моделът липсва"), QString())});

    QVERIFY2(panel.fixButtonAt(0) == nullptr,
             "without a fixer the row must stay exactly as before");

    // A fixer that handles nothing means the same: no button, no empty label.
    panel.setFixHandler(
        [](const Daqster::SceneOpenProblem&) -> QString { return QString(); },
        [](const Daqster::SceneOpenProblem&, QString*) { return false; });
    QVERIFY(panel.fixButtonAt(0) == nullptr);
}

void TestSceneProblemsPanel::fixerShowsLabelAndIsInvoked()
{
    QWidget host;
    SceneProblemsPanel panel(&host);
    const Daqster::SceneOpenProblem problem =
        makeProblem(QStringLiteral("4"), QStringLiteral("Chat"),
                    QStringLiteral("моделът липсва"), QStringLiteral("изтеглете го"));
    panel.setProblems({problem});

    int calls = 0;
    Daqster::SceneOpenProblem seen;
    panel.setFixHandler(
        [](const Daqster::SceneOpenProblem& p) {
            return p.message.contains(QStringLiteral("модел"))
                       ? QStringLiteral("Изтегли")
                       : QString();
        },
        [&](const Daqster::SceneOpenProblem& p, QString*) {
            ++calls;
            seen = p;
            return true;
        });

    QToolButton* button = panel.fixButtonAt(0);
    QVERIFY2(button != nullptr, "a handled problem gets the fixer's button");
    QCOMPARE(button->text(), QStringLiteral("Изтегли"));

    QSignalSpy spy(&panel, &SceneProblemsPanel::fixRequested);
    button->click();

    QCOMPARE(calls, 1);
    QCOMPARE(seen.nodeId, problem.nodeId);
    // The host is told after the click unwinds (it rebuilds the rows).
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<Daqster::SceneOpenProblem>().nodeId,
             problem.nodeId);
}

void TestSceneProblemsPanel::fixFailureIsReportedInPanel()
{
    QWidget host;
    SceneProblemsPanel panel(&host);
    panel.setProblems({makeProblem(QStringLiteral("5"), QStringLiteral("Tts"),
                                   QStringLiteral("voice липсва"), QString())});

    panel.setFixHandler(
        [](const Daqster::SceneOpenProblem&) { return QStringLiteral("Поправи"); },
        [](const Daqster::SceneOpenProblem&, QString* error) {
            if (error != nullptr)
                *error = QStringLiteral("няма достъп до /models");
            return false;
        });

    QSignalSpy spy(&panel, &SceneProblemsPanel::fixRequested);
    QToolButton* button = panel.fixButtonAt(0);
    QVERIFY(button != nullptr);
    button->click();

    QVERIFY2(panel.statusText().contains(QStringLiteral("няма достъп")),
             "the failure reason is shown in the panel, not swallowed");
    QCOMPARE(spy.count(), 0);
    QCOMPARE(panel.problemCount(), 1); // the row stays, the user may retry
}

void TestSceneProblemsPanel::panelRefreshesAfterFix()
{
    QWidget host;
    SceneProblemsPanel panel(&host);
    panel.setProblems({
        makeProblem(QStringLiteral("6"), QStringLiteral("Asr"),
                    QStringLiteral("моделът липсва"), QString()),
        makeProblem(QString(), QStringLiteral("сцена"),
                    QStringLiteral("няма ui секция"), QString()),
    });

    panel.setFixHandler(
        [](const Daqster::SceneOpenProblem&) { return QStringLiteral("Поправи"); },
        [](const Daqster::SceneOpenProblem&, QString*) { return true; });

    // The host's job: on success re-read the scene and refill the panel.
    connect(&panel, &SceneProblemsPanel::fixRequested, &panel, [&panel]() {
        panel.setProblems({makeProblem(QString(), QStringLiteral("сцена"),
                                       QStringLiteral("няма ui секция"), QString())});
    });

    QToolButton* button = panel.fixButtonAt(0);
    QVERIFY(button != nullptr);
    button->click();

    QTRY_COMPARE(panel.problemCount(), 1);
    QCOMPARE(panel.nodeIds(), QStringList({QString()}));
    QVERIFY(panel.statusText().isEmpty());
}

#include "test_scene_problems_panel.moc"