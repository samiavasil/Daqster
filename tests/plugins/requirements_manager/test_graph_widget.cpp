#include <QtTest>
#include <QApplication>

#include "DependencyGraphWidget.h"
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
    const QPointF afterStart = after.elementAt(0);
    const QPointF afterEnd = after.elementAt(1);

    QVERIFY2((afterStart - beforeStart - delta).manhattanLength() < 0.5,
             qPrintable(QStringLiteral("edge start must follow the moved from-node: "
                                       "before=(%1,%2) after=(%3,%4) delta=(%5,%6)")
                            .arg(beforeStart.x()).arg(beforeStart.y())
                            .arg(afterStart.x()).arg(afterStart.y())
                            .arg(delta.x()).arg(delta.y())));
    QVERIFY2((afterEnd - beforeEnd - delta).manhattanLength() < 0.5,
             qPrintable(QStringLiteral("edge end must follow the moved to-node: "
                                       "before=(%1,%2) after=(%3,%4) delta=(%5,%6)")
                            .arg(beforeEnd.x()).arg(beforeEnd.y())
                            .arg(afterEnd.x()).arg(afterEnd.y())
                            .arg(delta.x()).arg(delta.y())));
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

QTEST_MAIN(TestGraphWidget)
