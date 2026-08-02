#include "DependencyGraphWidget.h"

#include <QBrush>
#include <QGraphicsSceneMouseEvent>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QResizeEvent>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace Daqster {

namespace {

// Distance from the center of an axis-aligned rect to the rect boundary along
// the unit direction pointing from center toward otherCenter. Uses the actual
// per-node half-sizes (halfW/halfH) so arrowheads land on the node outline
// instead of a hardcoded radius.
qreal halfSizeAlongDirection(const QRectF &rect, const QPointF &center,
                             const QPointF &otherCenter)
{
    const QLineF line(center, otherCenter);
    const qreal length = line.length();
    if (length <= 0.0)
        return 0.0;
    const qreal ux = line.dx() / length;
    const qreal uy = line.dy() / length;
    const qreal halfW = rect.width() / 2.0;
    const qreal halfH = rect.height() / 2.0;
    const qreal ax = qAbs(ux);
    const qreal ay = qAbs(uy);
    if (ax < 1e-9)
        return (ay < 1e-9) ? 0.0 : halfH / ay;
    if (ay < 1e-9)
        return halfW / ax;
    return qMin(halfW / ax, halfH / ay);
}

// Builds the edge path (main line + filled arrowhead) between two node
// centers, shortened at both ends by the nodes' own half-sizes so the arrow
// is not hidden under the nodes.
QPainterPath makeEdgePath(const QRectF &fromRect, const QRectF &toRect,
                          const QPointF &fromCenter, const QPointF &toCenter)
{
    QLineF line(fromCenter, toCenter);
    const qreal length = line.length();
    const qreal arrowSize = 12.0;
    const qreal fromDist = halfSizeAlongDirection(fromRect, fromCenter, toCenter);
    const qreal toDist = halfSizeAlongDirection(toRect, toCenter, fromCenter);
    const qreal startT = (length <= 0.0) ? 0.0 : qMin(0.45, fromDist / length);
    const qreal endT = (length <= 0.0) ? 0.0 : qMin(0.55, (toDist + arrowSize) / length);
    const QPointF start = line.pointAt(startT);
    const QPointF end = line.pointAt(1.0 - endT);

    QPainterPath path(start);
    path.lineTo(end);

    // Arrowhead as a filled triangle pointing at "to".
    QLineF arrowLine(end, start);
    const qreal headAngle = 30.0;
    QLineF left(arrowLine);
    left.setAngle(left.angle() - headAngle);
    left.setLength(arrowSize);
    QLineF right(arrowLine);
    right.setAngle(right.angle() + headAngle);
    right.setLength(arrowSize);
    path.moveTo(left.p2());
    path.lineTo(end);
    path.lineTo(right.p2());
    path.closeSubpath();

    return path;
}

} // namespace

DependencyGraphScene::DependencyGraphScene(QObject *parent)
    : QGraphicsScene(parent)
{
}

void DependencyGraphScene::setRequirements(const QVector<Requirement> &requirements,
                                           const QVector<RequirementsValidator::Issue> &issues)
{
    Q_UNUSED(issues); // reserved for future per-node validation highlighting

    clear();
    m_nodeItems.clear();
    m_edgeItems.clear();

    m_data = DependencyGraphData::build(requirements);

    // Build + position every node first, then connect signals only after the
    // initial layout is complete so no positionChanged fires during the loop.
    for (const GraphNode &node : m_data.nodes()) {
        auto *item = new DependencyGraphNodeItem(node);
        item->setPos(node.pos.x() - item->boundingRect().width() / 2.0,
                     node.pos.y() - item->boundingRect().height() / 2.0);
        addItem(item);
        m_nodeItems.append(item);
        connect(item, &DependencyGraphNodeItem::clicked,
                this, &DependencyGraphScene::navigateRequested);
    }

    // Edges are live items holding their endpoint nodes. Each endpoint's
    // positionChanged drives updateGeometry() so the edge follows a drag in
    // real time. The same signal refreshes the scene rect so the view can
    // scroll to moved nodes (cheap at requirement scale).
    for (const GraphEdge &edge : m_data.edges()) {
        DependencyGraphNodeItem *from = m_nodeItems.at(edge.from);
        DependencyGraphNodeItem *to = m_nodeItems.at(edge.to);
        auto *item = new DependencyGraphEdgeItem(from, to);

        const QColor color = (edge.kind == GraphEdge::Parent)
            ? QColor(0x90A4AE) // blue-gray 400
            : QColor(0x455A64); // blue-gray 700
        QPen pen(color, (edge.kind == GraphEdge::Parent) ? 1.5 : 2.0);
        pen.setStyle((edge.kind == GraphEdge::Parent) ? Qt::DashLine : Qt::SolidLine);
        item->setPen(pen);
        item->setBrush(QBrush(color));
        item->setZValue(-1.0); // edges behind nodes
        addItem(item);
        m_edgeItems.append(item);

        connect(from, &DependencyGraphNodeItem::positionChanged,
                this, [item] { item->updateGeometry(); });
        connect(to, &DependencyGraphNodeItem::positionChanged,
                this, [item] { item->updateGeometry(); });
        item->updateGeometry(); // initial path for the current layout
    }

    // Refresh the scene rect after a drag so the view can scroll to moved
    // nodes (cheap at requirement scale). Connected LAST so the scene rect is
    // recomputed after the incident edges' updateGeometry() above on the same
    // positionChanged emission; connected after the initial setPos loop, so no
    // positionChanged fires during the initial layout.
    for (DependencyGraphNodeItem *item : m_nodeItems) {
        connect(item, &DependencyGraphNodeItem::positionChanged, this, [this] {
            setSceneRect(itemsBoundingRect().adjusted(-60, -60, 60, 60));
        });
    }

    setSceneRect(itemsBoundingRect().adjusted(-60, -60, 60, 60));
}

DependencyGraphNodeItem *DependencyGraphScene::nodeItemForId(const QString &id) const
{
    for (DependencyGraphNodeItem *item : m_nodeItems) {
        if (item->requirementId() == id)
            return item;
    }
    return nullptr;
}

QColor DependencyGraphScene::borderColorForStatus(const QString &status)
{
    if (status == QStringLiteral("ACTIVE"))
        return QColor(0x2E7D32); // green 800
    if (status == QStringLiteral("DONE"))
        return QColor(0x757575); // gray 600
    if (status == QStringLiteral("CANCELLED"))
        return QColor(0xC62828); // red 800
    return QColor(0x546E7A);     // blue-gray 600 (unknown status)
}

QColor DependencyGraphScene::fillColorForPriority(const QString &priority)
{
    if (priority == QStringLiteral("High"))
        return QColor(0x90A4AE); // darker fill
    if (priority == QStringLiteral("Low"))
        return QColor(0xCFD8DC); // lighter fill
    return QColor(0xB0BEC5);     // Medium (and unknown) — mid fill
}

DependencyGraphNodeItem::DependencyGraphNodeItem(const GraphNode &node, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_id(node.id)
    , m_title(node.title)
    , m_status(node.status)
    , m_priority(node.priority)
{
    const qreal width = qMax<qreal>(120.0, 36.0 + m_title.size() * 6.5);
    const qreal height = 48.0;
    m_rect = QRectF(0, 0, width, height);
    setFlags(ItemIsMovable | ItemSendsGeometryChanges);
    setToolTip(m_id);
}

QRectF DependencyGraphNodeItem::boundingRect() const
{
    return m_rect;
}

QVariant DependencyGraphNodeItem::itemChange(GraphicsItemChange change,
                                             const QVariant &value)
{
    // ItemSendsGeometryChanges is set in the constructor; this override makes
    // it real. Emitting from itemChange is safe here because the connected
    // slots only touch other items' paths (no geometry recursion).
    if (change == ItemPositionHasChanged)
        emit positionChanged();
    return QGraphicsObject::itemChange(change, value);
}

DependencyGraphEdgeItem::DependencyGraphEdgeItem(DependencyGraphNodeItem *from,
                                                 DependencyGraphNodeItem *to)
    : QGraphicsPathItem()
    , m_from(from)
    , m_to(to)
{
}

void DependencyGraphEdgeItem::updateGeometry()
{
    if (!m_from || !m_to)
        return;
    // Endpoints are computed in SCENE coordinates from the live items — never
    // from the stale layout data (pos() of a parented item, m_data, etc.).
    const QPointF fromCenter = m_from->scenePos() + m_from->boundingRect().center();
    const QPointF toCenter = m_to->scenePos() + m_to->boundingRect().center();
    setPath(makeEdgePath(m_from->boundingRect(), m_to->boundingRect(),
                         fromCenter, toCenter));
}

void DependencyGraphNodeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                    QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(QPen(DependencyGraphScene::borderColorForStatus(m_status), 2.0));
    painter->setBrush(QBrush(DependencyGraphScene::fillColorForPriority(m_priority)));
    painter->drawRoundedRect(m_rect, 8.0, 8.0);

    QFont idFont = painter->font();
    idFont.setBold(true);
    painter->setFont(idFont);
    painter->setPen(QColor(0x212121));
    painter->drawText(m_rect.adjusted(8, 4, -8, -4), Qt::AlignLeft | Qt::AlignTop, m_id);

    QFont titleFont = painter->font();
    titleFont.setBold(false);
    painter->setFont(titleFont);
    painter->setPen(QColor(0x424242));
    painter->drawText(m_rect.adjusted(8, 24, -8, -4), Qt::AlignLeft | Qt::AlignTop, m_title);
}

void DependencyGraphNodeItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    m_pressScenePos = event->scenePos();
    QGraphicsObject::mousePressEvent(event);
}

void DependencyGraphNodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    // A click (press+release without a drag) navigates to the requirement.
    if (event->button() == Qt::LeftButton
        && (event->scenePos() - m_pressScenePos).manhattanLength() < 5.0) {
        emit clicked(m_id);
    }
    QGraphicsObject::mouseReleaseEvent(event);
}

DependencyGraphView::DependencyGraphView(QWidget *parent)
    : QGraphicsView(parent)
{
}

void DependencyGraphView::setAutoFit(bool enabled)
{
    m_autoFit = enabled;
}

void DependencyGraphView::maybeFitToScene()
{
    if (!m_autoFit)
        return;
    if (!scene() || scene()->sceneRect().isEmpty())
        return;
    fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
}

void DependencyGraphView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    // The graph may have been populated before the window was resized/shown
    // (RequirementsManagerObject::Initialize), so refit while auto-fit is on.
    maybeFitToScene();
}

void DependencyGraphView::showEvent(QShowEvent *event)
{
    QGraphicsView::showEvent(event);
    maybeFitToScene();
}

void DependencyGraphView::wheelEvent(QWheelEvent *event)
{
    // User zoom takes over: stop refitting so later resizes do not clobber it.
    m_autoFit = false;
    const qreal factor = 1.15;
    const qreal zoom = (event->angleDelta().y() > 0) ? factor : 1.0 / factor;
    scale(zoom, zoom);
    event->accept();
}

void DependencyGraphView::mousePressEvent(QMouseEvent *event)
{
    // ScrollHandDrag scrolls the canvas on any drag, which would swallow node
    // drags (the node's ItemIsMovable flag would be dead). When the press lands
    // on a movable item, temporarily switch to NoDrag so the item receives the
    // move events and moves; presses on empty space / non-movable items keep
    // the scroll-hand pan behaviour.
    QGraphicsItem *item = itemAt(event->pos());
    if (item && (item->flags() & QGraphicsItem::ItemIsMovable)) {
        setDragMode(QGraphicsView::NoDrag);
        QGraphicsView::mousePressEvent(event);
        setDragMode(QGraphicsView::ScrollHandDrag);
    } else {
        QGraphicsView::mousePressEvent(event);
    }
}

DependencyGraphWidget::DependencyGraphWidget(QWidget *parent)
    : QWidget(parent)
{
    m_scene = new DependencyGraphScene(this);
    m_view = new DependencyGraphView(this);
    m_view->setScene(m_scene);
    m_view->setRenderHint(QPainter::Antialiasing, true);
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    m_view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

    m_warningLabel = new QLabel(this);
    m_warningLabel->setWordWrap(true);
    m_warningLabel->setVisible(false);

    // Color legend (AC4): status = border, priority = fill.
    QLabel *legend = new QLabel(this);
    legend->setTextFormat(Qt::RichText);
    legend->setText(
        QStringLiteral("<span style=\"color:#2E7D32;\">&#9632; ACTIVE</span>&nbsp;&nbsp;"
                       "<span style=\"color:#757575;\">&#9632; DONE</span>&nbsp;&nbsp;"
                       "<span style=\"color:#C62828;\">&#9632; CANCELLED</span>"
                       "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"
                       "<span style=\"background-color:#90A4AE;\">&nbsp;&nbsp;&nbsp;</span> High&nbsp;&nbsp;"
                       "<span style=\"background-color:#B0BEC5;\">&nbsp;&nbsp;&nbsp;</span> Medium&nbsp;&nbsp;"
                       "<span style=\"background-color:#CFD8DC;\">&nbsp;&nbsp;&nbsp;</span> Low"));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(m_view);
    layout->addWidget(legend);
    layout->addWidget(m_warningLabel);
    setLayout(layout);

    connect(m_scene, &DependencyGraphScene::navigateRequested,
            this, &DependencyGraphWidget::navigateRequested);
}

void DependencyGraphWidget::setRequirements(const QVector<Requirement> &requirements,
                                            const QVector<RequirementsValidator::Issue> &issues)
{
    m_scene->setRequirements(requirements, issues);

    const int dangling = m_scene->danglingCount();
    if (dangling > 0) {
        m_warningLabel->setText(
            tr("Warning: %1 dangling reference(s) — some Родител/Зависи от links "
               "point to requirements not present in this directory.")
                .arg(dangling));
        m_warningLabel->setVisible(true);
    } else {
        m_warningLabel->setVisible(false);
    }

    m_view->setAutoFit(true);
    if (!m_scene->sceneRect().isEmpty())
        m_view->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
}

} // namespace Daqster
