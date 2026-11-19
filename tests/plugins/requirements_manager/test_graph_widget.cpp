#include <QtTest>
#include <QApplication>
#include <QDir>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "DependencyGraphWidget.h"
#include "RequirementsParser.h"
#include "test_graph_widget.h"

using namespace Daqster;

namespace {

Requirement makeGuiRequirement(const QString &id, const QStringList &dependencies)
{
    Requirement req;
    req.id = id;
    req.title = id;
    req.status = QStringLiteral("ACTIVE");
    req.priority = QStringLiteral("Medium");
    req.section = QStringLiteral("active");
    req.dependencies = dependencies;
    return req;
}

// Two-node fixture: REQ-B depends on REQ-A. Kahn's algorithm over the
// dependency edge B -> A puts REQ-B on layer 0 and REQ-A on layer 1, both on
// the same row, so the single rendered edge is purely horizontal.
QVector<Requirement> makeGuiFixture()
{
    QVector<Requirement> reqs;
    reqs.append(makeGuiRequirement(QStringLiteral("REQ-A"), QStringList()));
    reqs.append(makeGuiRequirement(QStringLiteral("REQ-B"),
                                   QStringList() << QStringLiteral("REQ-A")));
    return reqs;
}

// Four-node fixture with distinct captions: R-1 (root) <- R-2 <- R-3, and R-4
// standing alone so "no incident edge" and "dimmed" can both be observed.
QVector<Requirement> makeSearchFixture()
{
    auto withTitle = [](const QString &id, const QString &title, const QStringList &deps) {
        Requirement req = makeGuiRequirement(id, deps);
        req.title = title;
        return req;
    };
    QVector<Requirement> reqs;
    reqs.append(withTitle(QStringLiteral("REQ-A"), QStringLiteral("Alpha tracker"), {}));
    reqs.append(withTitle(QStringLiteral("REQ-B"), QStringLiteral("Beta bridge"),
                          { QStringLiteral("REQ-A") }));
    reqs.append(withTitle(QStringLiteral("REQ-C"), QStringLiteral("Gamma node"),
                          { QStringLiteral("REQ-B") }));
    reqs.append(withTitle(QStringLiteral("REQ-D"), QStringLiteral("Delta node"), {}));
    return reqs;
}

// Viewport position of a node's centre (view may be zoomed/panned).
QPoint viewportCentre(DependencyGraphWidget &widget, DependencyGraphNodeItem *node)
{
    const QPointF sceneCentre = node->scenePos() + node->boundingRect().center();
    return widget.graphView()->mapFromScene(sceneCentre);
}

// Shows the widget at a usable size: an offscreen QWidget that was never shown
// has a 0-size viewport and mapFromScene() is meaningless.
void showWidget(DependencyGraphWidget &widget)
{
    widget.resize(900, 600);
    widget.show();
    QApplication::processEvents();
}

DependencyGraphEdgeItem *incidentEdge(DependencyGraphScene *scene,
                                      DependencyGraphNodeItem *node)
{
    for (DependencyGraphEdgeItem *edge : scene->edgeItems()) {
        if (edge->fromNode() == node || edge->toNode() == node)
            return edge;
    }
    return nullptr;
}

// Writes one parser-conformant requirement file (H1 "# ID: title" plus the
// "- **Ключ:** value" metadata the parser reads).
void writeRequirementFile(const QString &dir, int number, int dependsOn)
{
    const QString id = QStringLiteral("REQ-SW-PL-%1").arg(number, 3, 10, QLatin1Char('0'));
    const QString dependency = (dependsOn < 0)
        ? QStringLiteral("—")
        : QStringLiteral("REQ-SW-PL-%1").arg(dependsOn, 3, 10, QLatin1Char('0'));
    const QString body =
        QStringLiteral("# %1: Заглавие %2\n\n"
                       "- **Статус:** ACTIVE\n"
                       "- **Приоритет:** High\n"
                       "- **Дата:** 2026-01-01\n"
                       "- **Родител:** —\n"
                       "- **Зависи от:** %3\n\n"
                       "## Описание\n\ntext\n")
            .arg(id, QString::number(number), dependency);
    QFile file(QDir(dir).filePath(QStringLiteral("%1-test.md").arg(id)));
    QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Text), qPrintable(file.fileName()));
    file.write(body.toUtf8());
}

} // namespace

// Bug A regression test.
//
// Edges are stored as static QGraphicsPathItem paths built once from the
// static layout coordinates; the node's ItemSendsGeometryChanges flag was dead
// (no itemChange() override). Dragging a node therefore moved only the node
// while its incident edges stayed frozen in the old absolute geometry.
//
// This test records the edge path's main-line endpoints, moves both endpoint
// nodes by the same delta, then asserts the edge path tracked the move (in
// scene coordinates, since the edge item has no parent). It must FAIL before
// Fix A and PASS after it.
void TestGraphWidget::edgeFollowsNodeMove()
{
    DependencyGraphScene scene;
    scene.setRequirements(makeGuiFixture(), QVector<RequirementsValidator::Issue>());

    DependencyGraphNodeItem *nodeA = nullptr;
    DependencyGraphNodeItem *nodeB = nullptr;
    QGraphicsPathItem *edge = nullptr;
    const QList<QGraphicsItem *> items = scene.items();
    for (QGraphicsItem *item : items) {
        if (auto *node = dynamic_cast<DependencyGraphNodeItem *>(item)) {
            if (node->requirementId() == QStringLiteral("REQ-A"))
                nodeA = node;
            else
                nodeB = node;
        } else if (auto *path = dynamic_cast<QGraphicsPathItem *>(item)) {
            edge = path;
        }
    }
    QVERIFY2(nodeA, "node REQ-A must exist");
    QVERIFY2(nodeB, "node REQ-B must exist");
    QVERIFY2(edge, "a dependency edge must exist");
    QVERIFY2(edge->path().elementCount() >= 2, "edge path must contain a main line");

    const QPainterPath before = edge->path();
    const QPointF beforeStart = before.elementAt(0); // main-line start (from node)
    const QPointF beforeEnd = before.elementAt(1);   // main-line end (to node)

    const QPointF delta(50.0, 0.0);
    nodeA->setPos(nodeA->pos() + delta);
    nodeB->setPos(nodeB->pos() + delta);

    const QPainterPath after = edge->path();
    QCOMPARE(after.elementCount(), before.elementCount());
    QVERIFY2((after.elementAt(0) - beforeStart - delta).manhattanLength() < 0.01,
             "edge start must follow the moved node");
    QVERIFY2((after.elementAt(1) - beforeEnd - delta).manhattanLength() < 0.01,
             "edge end must follow the moved node");
}

// Bug B regression test.
//
// RequirementsManagerObject::Initialize() calls openDirectory() -> reload() ->
// graph setRequirements() BEFORE m_Win->resize(1100,700) and show(), so the
// fitInView() inside setRequirements() computes a zoom for the tiny default
// viewport. Without a resizeEvent/showEvent refit the graph stays frozen as a
// small cluster in the middle of the enlarged window.
//
// This test mirrors that order: populate the graph while the widget still has
// its default size, then resize + show. The view transform must end up equal
// to a fit-in-view of the scene rect for the *current* viewport size. It must
// FAIL before Fix B and PASS after it.
void TestGraphWidget::fitsViewportAfterResize()
{
    DependencyGraphWidget widget;
    widget.setRequirements(makeGuiFixture(), QVector<RequirementsValidator::Issue>());

    widget.resize(900, 600);
    widget.show();
    QApplication::processEvents();

    auto *view = widget.findChild<QGraphicsView *>();
    QVERIFY2(view, "graph view must exist");

    const QRectF sceneRect = view->scene()->sceneRect();
    QVERIFY2(!sceneRect.isEmpty(), "scene rect must be non-empty");
    QVERIFY2(view->viewport()->width() > 0 && view->viewport()->height() > 0,
             "viewport must have a size after show");

    const qreal expected =
        qMin(static_cast<qreal>(view->viewport()->width()) / sceneRect.width(),
             static_cast<qreal>(view->viewport()->height()) / sceneRect.height());
    const qreal actual = view->transform().m11();
    QVERIFY2(qAbs(actual - expected) < 0.05,
             qPrintable(QStringLiteral("view must fit the scene to the viewport after "
                                       "resize/show: expected scale=%1 actual=%2")
                            .arg(expected).arg(actual)));
}

// Search is by ID OR caption, case-insensitive, and the counter says how many
// hits the query has (user item 6: "3 / 12" next to the box).
void TestGraphWidget::searchFindsByIdAndCaption()
{
    DependencyGraphWidget widget;
    widget.setRequirements(makeSearchFixture(), QVector<RequirementsValidator::Issue>());
    QVERIFY(widget.searchEdit());
    QVERIFY(widget.matchCounter());

    QVERIFY(widget.counterText().isEmpty()); // no query, no noise

    widget.searchEdit()->setText(QStringLiteral("req-b"));
    QCOMPARE(widget.matchCount(), 1);
    QCOMPARE(widget.counterText(), QStringLiteral("1 / 1"));

    widget.searchEdit()->setText(QStringLiteral("gamma"));
    QCOMPARE(widget.matchCount(), 1);
    QCOMPARE(widget.counterText(), QStringLiteral("1 / 1"));
    QVERIFY(widget.graphScene()->nodeItemForId(QStringLiteral("REQ-C"))->searchMatch());

    // No hit at all: the counter must say 0, not stay at the old number.
    widget.searchEdit()->setText(QStringLiteral("zzz"));
    QCOMPARE(widget.matchCount(), 0);
    QCOMPARE(widget.counterText(), QStringLiteral("0 / 0"));
}

// Enter / „Следващ" walk the matches cyclically and the current hit is centred
// in the viewport (a centred-but-20%-wide node is not "in view").
void TestGraphWidget::searchNextCyclesAndCentres()
{
    DependencyGraphWidget widget;
    widget.setRequirements(makeSearchFixture(), QVector<RequirementsValidator::Issue>());
    showWidget(widget);

    widget.searchEdit()->setText(QStringLiteral("REQ-")); // all four
    QCOMPARE(widget.matchCount(), 4);
    QCOMPARE(widget.counterText(), QStringLiteral("1 / 4"));
    QCOMPARE(widget.graphScene()->nodeItems().size(), 4);

    widget.searchNext();
    QCOMPARE(widget.counterText(), QStringLiteral("2 / 4"));
    widget.searchNext();
    QCOMPARE(widget.counterText(), QStringLiteral("3 / 4"));
    widget.searchNext();
    QCOMPARE(widget.counterText(), QStringLiteral("4 / 4"));
    widget.searchNext(); // wraps
    QCOMPARE(widget.counterText(), QStringLiteral("1 / 4"));

    // Exactly one node carries the "current match" emphasis, and it is the one
    // the counter names.
    int current = 0;
    DependencyGraphNodeItem *currentItem = nullptr;
    for (DependencyGraphNodeItem *item : widget.graphScene()->nodeItems()) {
        if (item->searchCurrent()) {
            ++current;
            currentItem = item;
        }
    }
    QCOMPARE(current, 1);
    QVERIFY(currentItem);
    QCOMPARE(currentItem->requirementId(), QStringLiteral("REQ-A"));

    // The hit is brought into view at a readable zoom: the node's whole body is
    // inside the viewport and the view is not left at the whole-graph fit.
    const QRect nodeInView =
        widget.graphView()->mapFromScene(currentItem->sceneBoundingRect()).boundingRect();
    QVERIFY2(widget.graphView()->viewport()->rect().contains(nodeInView),
             qPrintable(QStringLiteral("match must be in view: node=%1,%2 %3x%4 viewport=%5x%6")
                            .arg(nodeInView.x()).arg(nodeInView.y())
                            .arg(nodeInView.width()).arg(nodeInView.height())
                            .arg(widget.graphView()->viewport()->width())
                            .arg(widget.graphView()->viewport()->height())));
    QVERIFY2(widget.graphView()->transform().m11() >= 0.6,
             qPrintable(QStringLiteral("match must be zoomed in, scale=%1")
                            .arg(widget.graphView()->transform().m11())));
}

// A single click selects and must NOT navigate; the double click keeps the
// navigation the widget had before (user item 6).
void TestGraphWidget::singleClickSelectsWithoutNavigating()
{
    DependencyGraphWidget widget;
    widget.setRequirements(makeSearchFixture(), QVector<RequirementsValidator::Issue>());
    showWidget(widget);

    QSignalSpy navigation(&widget, &DependencyGraphWidget::navigateRequested);
    DependencyGraphNodeItem *nodeB = widget.graphScene()->nodeItemForId(QStringLiteral("REQ-B"));
    QVERIFY(nodeB);

    QTest::mouseClick(widget.graphView()->viewport(), Qt::LeftButton, Qt::NoModifier,
                      viewportCentre(widget, nodeB));
    QApplication::processEvents();

    QCOMPARE(navigation.count(), 0); // selection never navigates
    QCOMPARE(widget.graphScene()->selectedNode(), nodeB);
    QCOMPARE(nodeB->emphasis(), DependencyGraphNodeItem::Emphasis::Selected);
}

void TestGraphWidget::doubleClickNavigates()
{
    DependencyGraphWidget widget;
    widget.setRequirements(makeSearchFixture(), QVector<RequirementsValidator::Issue>());
    showWidget(widget);

    QSignalSpy navigation(&widget, &DependencyGraphWidget::navigateRequested);
    DependencyGraphNodeItem *nodeC = widget.graphScene()->nodeItemForId(QStringLiteral("REQ-C"));
    QVERIFY(nodeC);

    QTest::mouseDClick(widget.graphView()->viewport(), Qt::LeftButton, Qt::NoModifier,
                       viewportCentre(widget, nodeC));
    QApplication::processEvents();

    QCOMPARE(navigation.count(), 1);
    QCOMPARE(navigation.first().at(0).toString(), QStringLiteral("REQ-C"));
}

void TestGraphWidget::selectionHighlightsIncidentEdges()
{
    DependencyGraphWidget widget;
    widget.setRequirements(makeSearchFixture(), QVector<RequirementsValidator::Issue>());
    showWidget(widget);

    DependencyGraphScene *scene = widget.graphScene();
    DependencyGraphNodeItem *nodeB = scene->nodeItemForId(QStringLiteral("REQ-B"));
    DependencyGraphNodeItem *nodeA = scene->nodeItemForId(QStringLiteral("REQ-A"));
    DependencyGraphNodeItem *nodeD = scene->nodeItemForId(QStringLiteral("REQ-D"));
    QVERIFY(nodeB && nodeA && nodeD);

    DependencyGraphEdgeItem *edgeBA = incidentEdge(scene, nodeB);
    QVERIFY(edgeBA);
    const qreal normalWidth = edgeBA->pen().widthF();
    const QColor normalColour = edgeBA->pen().color();

    scene->selectNode(nodeB);
    QCOMPARE(nodeB->emphasis(), DependencyGraphNodeItem::Emphasis::Selected);
    QCOMPARE(nodeA->emphasis(), DependencyGraphNodeItem::Emphasis::Neighbour);
    QCOMPARE(nodeD->emphasis(), DependencyGraphNodeItem::Emphasis::Dimmed);
    QVERIFY2(nodeD->opacity() < 0.5, "unrelated nodes must be dimmed");

    QCOMPARE(edgeBA->emphasis(), DependencyGraphEdgeItem::Emphasis::Highlighted);
    QVERIFY2(edgeBA->pen().widthF() > normalWidth, "a highlighted edge must be thicker");
    QCOMPARE(edgeBA->pen().color(),
             DependencyGraphScene::borderColorForStatus(nodeB->status()).lighter(125));
    QVERIFY(edgeBA->pen().color() != normalColour);
    QCOMPARE(edgeBA->opacity(), 1.0);

    // Escape clears the selection on the view (the user-facing way out).
    widget.graphView()->setFocus();
    QTest::keyClick(widget.graphView(), Qt::Key_Escape);
    QApplication::processEvents();
    QVERIFY(!scene->selectedNode());
    QCOMPARE(edgeBA->emphasis(), DependencyGraphEdgeItem::Emphasis::Normal);
    QCOMPARE(edgeBA->pen().widthF(), normalWidth);
    QCOMPARE(nodeD->opacity(), 1.0);
}

void TestGraphWidget::statusColoursAreTableDriven_data()
{
    QTest::addColumn<QString>("status");
    QTest::addColumn<int>("rgb");

    QTest::newRow("ACTIVE") << "ACTIVE" << int(0x2E7D32);
    QTest::newRow("IMPLEMENTED") << "IMPLEMENTED" << int(0x1565C0);
    QTest::newRow("DONE") << "DONE" << int(0x757575);
    QTest::newRow("CANCELLED") << "CANCELLED" << int(0xC62828);
    QTest::newRow("BACKLOG") << "BACKLOG" << int(0xEF6C00);
}

void TestGraphWidget::statusColoursAreTableDriven()
{
    QFETCH(QString, status);
    QFETCH(int, rgb);

    const QColor expected(rgb);
    QCOMPARE(DependencyGraphScene::borderColorForStatus(status), expected);

    bool inTable = false;
    for (const QPair<QString, QColor> &style : DependencyGraphScene::statusStyles())
        inTable = inTable || (style.first == status && style.second == expected);
    QVERIFY2(inTable, "the status colour must come from the one legend table");
    QCOMPARE(DependencyGraphScene::statusStyles().size(), 5);
}

// The tooltip carries id, title, status, dependency count and the issues the
// widget used to discard.
void TestGraphWidget::tooltipHasStatusAndIssues()
{
    RequirementsValidator::Issue issue;
    issue.id = QStringLiteral("REQ-A");
    issue.field = QStringLiteral("status");
    issue.message = QStringLiteral("невалиден статус");
    issue.severity = RequirementsValidator::Severity::Error;
    issue.filePath = QStringLiteral("/tmp/REQ-A-test.md");

    DependencyGraphWidget widget;
    widget.setRequirements(makeSearchFixture(), { issue });

    DependencyGraphNodeItem *nodeA = widget.graphScene()->nodeItemForId(QStringLiteral("REQ-A"));
    QVERIFY(nodeA);
    const QString tip = nodeA->toolTip();
    QVERIFY2(tip.contains(QStringLiteral("REQ-A")), qPrintable(tip));
    QVERIFY2(tip.contains(QStringLiteral("Alpha tracker")), qPrintable(tip));
    QVERIFY2(tip.contains(QStringLiteral("Статус: ACTIVE")), qPrintable(tip));
    QVERIFY2(tip.contains(QStringLiteral("Зависимости: 0")), qPrintable(tip));
    QVERIFY2(tip.contains(QStringLiteral("невалиден статус")), qPrintable(tip));

    // Counts come from the parsed requirement, not from the item's own guess.
    DependencyGraphNodeItem *nodeB = widget.graphScene()->nodeItemForId(QStringLiteral("REQ-B"));
    QVERIFY(nodeB);
    QCOMPARE(nodeB->dependencyCount(), 1);
    QVERIFY(nodeB->toolTip().contains(QStringLiteral("Проблеми: няма")));
}

// The real parse + layout path on a 60-node tree: the graph is usable at
// requirement-tree scale and an exact ID finds exactly one node.
void TestGraphWidget::largeTreeSearchesOneNode()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString activeDir =
        QDir(tmp.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    QVERIFY(QDir().mkpath(activeDir));
    for (int n = 100; n < 160; ++n)
        writeRequirementFile(activeDir, n, n == 100 ? -1 : n - 1);

    const QVector<Requirement> reqs = RequirementsParser::parseDirectory(tmp.path());
    QCOMPARE(reqs.size(), 60);

    DependencyGraphWidget widget;
    widget.setRequirements(reqs, RequirementsValidator::validate(reqs));

    QVERIFY2(widget.graphScene()->nodeCount() > 50,
             qPrintable(QStringLiteral("expected a large graph, got %1 nodes")
                            .arg(widget.graphScene()->nodeCount())));

    widget.searchEdit()->setText(QStringLiteral("REQ-SW-PL-123"));
    QCOMPARE(widget.matchCount(), 1);
    QCOMPARE(widget.counterText(), QStringLiteral("1 / 1"));
    QCOMPARE(widget.graphScene()->nodeItemForId(QStringLiteral("REQ-SW-PL-123"))
                 ->searchCurrent(),
             true);
}

QTEST_MAIN(TestGraphWidget)
