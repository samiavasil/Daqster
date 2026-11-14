#include "test_live_validation_problems.h"

#include "NodeProblems.h"
#include "SceneProblemsPanel.h"

#include <QApplication>
#include <QListWidget>
#include <QString>

// A real NodeDelegateModel so `setValidationState()` takes the same path a node
// takes at runtime (including its "nothing changed, do not request an update"
// fast path). Only the pure virtuals are implemented — the validation state is
// what this test is about.
namespace {

class TestValidationNode : public QtNodes::NodeDelegateModel
{
public:
    QtNodes::NodeDataType dataType(QtNodes::PortType, QtNodes::PortIndex) const override
    {
        return QtNodes::NodeDataType{QStringLiteral("test"), QStringLiteral("Test")};
    }
    unsigned int nPorts(QtNodes::PortType) const override { return 0; }
    void setInData(std::shared_ptr<QtNodes::NodeData>, QtNodes::PortIndex) override {}
    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex) override
    {
        return nullptr;
    }
    QWidget* embeddedWidget() override { return nullptr; }
    QString caption() const override { return QStringLiteral("Chat"); }
    QString name() const override { return QStringLiteral("TestValidationNode"); }
};

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

void TestLiveValidationProblems::liveValidationErrorsAppearInPanel()
{
    TestValidationNode node;
    node.setValidationState(
        {QtNodes::NodeValidationState::State::Error, QStringLiteral("моделът липсва")});

    const QVector<NodeProblems::NodeState> states{
        {7, QStringLiteral("Chat"), node.validationState()}};
    const QVector<Daqster::SceneOpenProblem> live = NodeProblems::liveProblems(states);
    QCOMPARE(live.size(), 1);
    QCOMPARE(live.first().nodeId, QStringLiteral("7"));
    QCOMPARE(live.first().message, QStringLiteral("моделът липсва"));

    QWidget host;
    SceneProblemsPanel panel(&host);
    panel.setProblems(NodeProblems::merge({}, states));

    QVERIFY2(!panel.isHidden(), "a live validation error must be visible, not a tooltip");
    QCOMPARE(panel.problemCount(), 1);
    QVERIFY2(panel.listWidget()->item(0)->text().contains(QStringLiteral("моделът липсва")),
             "the panel must show the node's own message");
}

void TestLiveValidationProblems::panelDeduplicatesSameProblem()
{
    TestValidationNode node;
    // applySceneProblems() marks the node with the verdict text, so the live
    // state carries exactly the same message as the open-time problem.
    const QString text = QStringLiteral("Chat (3): моделът липсва — изтеглете го");
    node.setValidationState({QtNodes::NodeValidationState::State::Error, text});

    const QVector<NodeProblems::NodeState> states{
        {3, QStringLiteral("Chat"), node.validationState()}};
    const QVector<Daqster::SceneOpenProblem> merged = NodeProblems::merge(
        {makeProblem(QStringLiteral("3"), QStringLiteral("Chat"),
                     text, QStringLiteral("изтеглете го"))},
        states);

    QCOMPARE(merged.size(), 1);
    QVERIFY2(merged.first().hint == QStringLiteral("изтеглете го"),
             "the richer copy (with the hint) must survive the merge");

    QWidget host;
    SceneProblemsPanel panel(&host);
    panel.setProblems(merged);
    QCOMPARE(panel.problemCount(), 1);
}

void TestLiveValidationProblems::panelClearsWhenNodeBecomesValid()
{
    TestValidationNode node;
    node.setValidationState(
        {QtNodes::NodeValidationState::State::Error, QStringLiteral("backend липсва")});

    QWidget host;
    SceneProblemsPanel panel(&host);
    panel.setProblems(NodeProblems::merge(
        {}, {{5, QStringLiteral("Tts"), node.validationState()}}));
    QCOMPARE(panel.problemCount(), 1);

    node.setValidationState({QtNodes::NodeValidationState::State::Valid, QString()});

    const QVector<Daqster::SceneOpenProblem> merged =
        NodeProblems::merge({}, {{5, QStringLiteral("Tts"), node.validationState()}});
    QVERIFY2(merged.isEmpty(), "a valid node must not leave a stale problem behind");

    panel.setProblems(merged);
    QCOMPARE(panel.problemCount(), 0);
    QVERIFY2(panel.isHidden(), "an empty list must give the canvas its space back");
}
