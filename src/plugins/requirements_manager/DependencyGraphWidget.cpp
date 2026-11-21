#include "DependencyGraphWidget.h"

#include <QBrush>
#include <QEvent>
#include <QGraphicsSceneMouseEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QResizeEvent>
#include <QSet>
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

// The ID:field fallback the validation dialog uses, so a tooltip issue reads
// the same way as the report one.
QString issueLine(const RequirementsValidator::Issue &issue)
{
    const QString token = (issue.severity == RequirementsValidator::Severity::Error)
                              ? QStringLiteral("грешка")
                              : QStringLiteral("предупреждение");
    QString where = issue.field;
    if (issue.id.isEmpty())
        where.clear();
    return where.isEmpty() ? QStringLiteral("%1: %2").arg(token, issue.message)
                           : QStringLiteral("%1 (%2): %3").arg(token, where, issue.message);
}

} // namespace

DependencyGraphScene::DependencyGraphScene(QObject *parent)
    : QGraphicsScene(parent)
{
}

void DependencyGraphScene::setRequirements(const QVector<Requirement> &requirements,
                                           const QVector<RequirementsValidator::Issue> &issues)
{
    clear();
    m_nodeItems.clear();
    m_edgeItems.clear();
    m_issuesById.clear();
    m_selected = nullptr;

    m_data = DependencyGraphData::build(requirements);

    for (const RequirementsValidator::Issue &issue : issues) {
        if (issue.id.trimmed().isEmpty())
            continue;
        m_issuesById[issue.id.trimmed()].append(issueLine(issue));
    }

    // Build + position every node first, then connect signals only after the
    // initial layout is complete so no positionChanged fires during the loop.
    for (const GraphNode &node : m_data.nodes()) {
        auto *item = new DependencyGraphNodeItem(node);
        item->setPos(node.pos.x() - item->boundingRect().width() / 2.0,
                     node.pos.y() - item->boundingRect().height() / 2.0);
        const Requirement &req = requirements.at(node.reqIndex);
        item->setDependencyInfo(req.dependencies.size(), req.parentIds.size(),
                                m_issuesById.value(node.id));
        addItem(item);
        m_nodeItems.append(item);
        // Single click selects, double click navigates (user item 6).
        connect(item, &DependencyGraphNodeItem::selected,
                this, [this](const QString &id) { selectNode(nodeItemForId(id)); });
        connect(item, &DependencyGraphNodeItem::activated,
                this, &DependencyGraphScene::navigateRequested);
    }

    // Edges are live items holding their endpoint nodes. Each endpoint's
    // positionChanged drives updateGeometry() so the edge follows a drag in
    // real time.
    for (const GraphEdge &edge : m_data.edges()) {
        auto *item = new DependencyGraphEdgeItem(m_nodeItems.at(edge.from),
                                                 m_nodeItems.at(edge.to), edge.kind);
        item->setZValue(-1.0); // edges behind nodes
        addItem(item);
        m_edgeItems.append(item);

        connect(item->fromNode(), &DependencyGraphNodeItem::positionChanged,
                this, [item] { item->updateGeometry(); });
        connect(item->toNode(), &DependencyGraphNodeItem::positionChanged,
                this, [item] { item->updateGeometry(); });
        item->updateGeometry(); // initial path for the current layout
    }

    // Refresh the scene rect after a drag so the view can scroll to moved
    // nodes (cheap at requirement scale). Connected LAST so the scene rect is
    // recomputed after the incident edges' updateGeometry() above.
    for (DependencyGraphNodeItem *item : m_nodeItems) {
        connect(item, &DependencyGraphNodeItem::positionChanged, this, [this] {
            setSceneRect(itemsBoundingRect().adjusted(-60, -60, 60, 60));
        });
    }

    setSceneRect(itemsBoundingRect().adjusted(-60, -60, 60, 60));
}

DependencyGraphNodeItem *DependencyGraphScene::nodeItemForId(const QString &id,
                                                             const QString &repo) const
{
    for (DependencyGraphNodeItem *item : m_nodeItems) {
        if (item->requirementId() != id)
            continue;
        // With duplicate IDs across repos (REQ-SW-PL-012), an explicit repo
        // disambiguates; empty repo keeps the old first-match behaviour.
        if (!repo.isEmpty() && item->repo() != repo)
            continue;
        return item;
    }
    return nullptr;
}

QVector<DependencyGraphNodeItem *> DependencyGraphScene::nodesMatching(const QString &text) const
{
    QVector<DependencyGraphNodeItem *> matches;
    const QString needle = text.trimmed();
    if (needle.isEmpty())
        return matches;
    for (DependencyGraphNodeItem *item : m_nodeItems) {
        if (item->requirementId().contains(needle, Qt::CaseInsensitive)
            || item->title().contains(needle, Qt::CaseInsensitive)) {
            matches.append(item);
        }
    }
    return matches;
}

QStringList DependencyGraphScene::issuesFor(const QString &id) const
{
    return m_issuesById.value(id);
}

void DependencyGraphScene::selectNode(DependencyGraphNodeItem *node)
{
    if (!node)
        return clearHighlight();

    QSet<DependencyGraphNodeItem *> neighbours;
    for (DependencyGraphEdgeItem *edge : m_edgeItems) {
        const bool incident = (edge->fromNode() == node || edge->toNode() == node);
        edge->setEmphasis(incident ? DependencyGraphEdgeItem::Emphasis::Highlighted
                                   : DependencyGraphEdgeItem::Emphasis::Dimmed);
        if (incident) {
            neighbours.insert(edge->fromNode());
            neighbours.insert(edge->toNode());
        }
    }
    neighbours.remove(node);

    for (DependencyGraphNodeItem *item : m_nodeItems) {
        if (item == node)
            item->setEmphasis(DependencyGraphNodeItem::Emphasis::Selected);
        else if (neighbours.contains(item))
            item->setEmphasis(DependencyGraphNodeItem::Emphasis::Neighbour);
        else
            item->setEmphasis(DependencyGraphNodeItem::Emphasis::Dimmed);
    }
    m_selected = node;
}

void DependencyGraphScene::clearHighlight()
{
    for (DependencyGraphNodeItem *item : m_nodeItems)
        item->setEmphasis(DependencyGraphNodeItem::Emphasis::Normal);
    for (DependencyGraphEdgeItem *item : m_edgeItems)
        item->setEmphasis(DependencyGraphEdgeItem::Emphasis::Normal);
    m_selected = nullptr;
}

void DependencyGraphScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    // A press that lands on empty canvas drops the selection; presses on a
    // node/edge keep it (the node itself decides between select and drag).
    if (!itemAt(event->scenePos(), QTransform()))
        clearHighlight();
    QGraphicsScene::mousePressEvent(event);
}

QColor DependencyGraphScene::borderColorForStatus(const QString &status)
{
    for (const QPair<QString, QColor> &style : statusStyles()) {
        if (status == style.first)
            return style.second;
    }
    return QColor(0x546E7A); // blue-gray 600 (unknown status)
}

QVector<QPair<QString, QColor>> DependencyGraphScene::statusStyles()
{
    return { qMakePair(QStringLiteral("DRAFT"), QColor(0x6A1B9A)),       // purple 800
             qMakePair(QStringLiteral("ACTIVE"), QColor(0x2E7D32)),      // green 800
             qMakePair(QStringLiteral("IMPLEMENTED"), QColor(0x1565C0)), // blue 800
             qMakePair(QStringLiteral("DONE"), QColor(0x757575)),        // gray 600
             qMakePair(QStringLiteral("CANCELLED"), QColor(0xC62828)),   // red 800
             qMakePair(QStringLiteral("BACKLOG"), QColor(0xEF6C00)) };   // orange 800
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
    , m_repo(node.repo)
    , m_title(node.title)
    , m_status(node.status)
    , m_priority(node.priority)
{
    // Visual width is computed once in DependencyGraphData::build() so the
    // headless layout and the GUI widget always agree (height stays fixed).
    m_rect = QRectF(0, 0, node.width, 48.0);
    setFlags(ItemIsMovable | ItemSendsGeometryChanges);
    composeTooltip();
}

void DependencyGraphNodeItem::setDependencyInfo(int dependencyCount, int parentCount,
                                                const QStringList &issues)
{
    m_dependencyCount = dependencyCount;
    m_parentCount = parentCount;
    m_issues = issues;
    composeTooltip();
}

void DependencyGraphNodeItem::composeTooltip()
{
    m_tooltipLines.clear();
    m_tooltipLines.append(m_repo.isEmpty() ? m_id
                                           : QStringLiteral("%1 (%2)").arg(m_id, m_repo));
    if (!m_title.isEmpty())
        m_tooltipLines.append(m_title);
    m_tooltipLines.append(QStringLiteral("Статус: %1 · Приоритет: %2")
                              .arg(m_status.isEmpty() ? QStringLiteral("—") : m_status,
                                   m_priority.isEmpty() ? QStringLiteral("—") : m_priority));
    m_tooltipLines.append(QStringLiteral("Зависимости: %1 · Родители: %2")
                              .arg(m_dependencyCount)
                              .arg(m_parentCount));
    if (m_issues.isEmpty()) {
        m_tooltipLines.append(QStringLiteral("Проблеми: няма"));
    } else {
        m_tooltipLines.append(QStringLiteral("Проблеми (%1):").arg(m_issues.size()));
        for (const QString &issue : m_issues)
            m_tooltipLines.append(QStringLiteral("• %1").arg(issue));
    }
    setToolTip(m_tooltipLines.join(QLatin1Char('\n')));
}

QRectF DependencyGraphNodeItem::boundingRect() const
{
    // The halo and the search ring paint outside m_rect; the bounding rect
    // covers them so they are not clipped.
    return m_rect.adjusted(-6, -6, 6, 6);
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

void DependencyGraphNodeItem::setEmphasis(Emphasis emphasis)
{
    m_emphasis = emphasis;
    // Dimming is an opacity of the whole node; the halo/neighbour emphasis is
    // painted in paint().
    setOpacity(emphasis == Emphasis::Dimmed ? 0.3 : 1.0);
    update();
}

void DependencyGraphNodeItem::setSearchState(bool match, bool current)
{
    if (m_searchMatch == match && m_searchCurrent == current)
        return;
    m_searchMatch = match;
    m_searchCurrent = current;
    update();
}

void DependencyGraphEdgeItem::applyStyle()
{
    const bool dashed = (m_kind == GraphEdge::Parent);
    QColor color = dashed ? QColor(0x90A4AE) : QColor(0x455A64);
    qreal width = dashed ? 1.5 : 2.0;
    qreal opacityValue = 1.0;

    switch (m_emphasis) {
    case Emphasis::Normal:
        break;
    case Emphasis::Highlighted:
        // The edge inherits the SOURCE (child) status colour, brightened so a
        // highlighted link reads as an accent, not as another node outline.
        color = m_from ? DependencyGraphScene::borderColorForStatus(m_from->status())
                             .lighter(125)
                       : color;
        width += 1.6;
        break;
    case Emphasis::Dimmed:
        opacityValue = 0.18;
        break;
    }

    QPen pen(color, width);
    pen.setStyle(dashed ? Qt::DashLine : Qt::SolidLine);
    pen.setCapStyle(Qt::RoundCap);
    setPen(pen);
    setBrush(QBrush(color));
    setOpacity(opacityValue);
}

void DependencyGraphNodeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                                    QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    painter->setRenderHint(QPainter::Antialiasing, true);
    const QColor border = DependencyGraphScene::borderColorForStatus(m_status);

    if (m_emphasis == Emphasis::Selected) {
        QColor halo = border;
        halo.setAlpha(70);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(halo));
        painter->drawRoundedRect(m_rect.adjusted(-5, -5, 5, 5), 11.0, 11.0);
    }
    if (m_searchMatch) {
        QPen ring(QColor(0xEF6C00), m_searchCurrent ? 2.4 : 1.4, Qt::DashLine);
        painter->setPen(ring);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(m_rect.adjusted(-3.5, -3.5, 3.5, 3.5), 10.0, 10.0);
    }

    qreal penWidth = 2.0;
    if (m_emphasis == Emphasis::Selected)
        penWidth = 3.6;
    else if (m_emphasis == Emphasis::Neighbour)
        penWidth = 2.8;

    painter->setPen(QPen(border, penWidth));
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
    // A click (press+release without a drag) SELECTS the node. Navigation is
    // reserved for a double click so a look around never changes tabs.
    if (event->button() == Qt::LeftButton
        && (event->scenePos() - m_pressScenePos).manhattanLength() < 5.0) {
        emit selected(m_id);
    }
    QGraphicsObject::mouseReleaseEvent(event);
}

void DependencyGraphNodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit activated(m_id);
        event->accept();
        return;
    }
    QGraphicsObject::mouseDoubleClickEvent(event);
}

DependencyGraphEdgeItem::DependencyGraphEdgeItem(DependencyGraphNodeItem *from,
                                                 DependencyGraphNodeItem *to,
                                                 GraphEdge::Kind kind)
    : QGraphicsPathItem()
    , m_from(from)
    , m_to(to)
    , m_kind(kind)
{
    applyStyle();
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

void DependencyGraphEdgeItem::setEmphasis(Emphasis emphasis)
{
    m_emphasis = emphasis;
    applyStyle();
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

void DependencyGraphView::fitAll()
{
    if (!scene() || scene()->sceneRect().isEmpty())
        return;
    m_autoFit = true;
    fitInView(scene()->sceneRect(), Qt::KeepAspectRatio);
}

void DependencyGraphView::resetZoom()
{
    m_autoFit = false;
    resetTransform();
    if (scene())
        centerOn(scene()->sceneRect().center());
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

void DependencyGraphView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit escapePressed();
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
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
        if (!item)
            emit emptyAreaPressed();
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
    m_view->setFocusPolicy(Qt::StrongFocus);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Търси по ID или заглавие…"));
    m_searchEdit->setClearButtonEnabled(true);
    m_nextButton = new QPushButton(tr("Следващ"), this);
    m_matchCounter = new QLabel(this);
    QPushButton *fitButton = new QPushButton(tr("Покажи всичко"), this);
    QPushButton *resetButton = new QPushButton(tr("Нулирай"), this);

    QHBoxLayout *searchRow = new QHBoxLayout;
    searchRow->setContentsMargins(0, 0, 0, 0);
    searchRow->addWidget(new QLabel(tr("Търсене:"), this));
    searchRow->addWidget(m_searchEdit, 1);
    searchRow->addWidget(m_nextButton);
    searchRow->addWidget(m_matchCounter);
    searchRow->addSpacing(8);
    searchRow->addWidget(fitButton);
    searchRow->addWidget(resetButton);

    m_legend = new QLabel(m_view);
    m_legend->setTextFormat(Qt::RichText);
    m_legend->setText(buildLegendHtml());
    m_legend->setStyleSheet(QStringLiteral(
        "background-color: rgba(255,255,255,215); border: 1px solid #B0BEC5;"
        " border-radius: 4px; padding: 3px 6px;"));
    m_legend->adjustSize();
    m_legend->raise();
    m_view->installEventFilter(this);

    m_warningLabel = new QLabel(this);
    m_warningLabel->setWordWrap(true);
    m_warningLabel->setVisible(false);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addLayout(searchRow);
    layout->addWidget(m_view, 1);
    layout->addWidget(m_warningLabel);
    setLayout(layout);

    connect(m_scene, &DependencyGraphScene::navigateRequested,
            this, &DependencyGraphWidget::navigateRequested);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &DependencyGraphWidget::runSearch);
    connect(m_searchEdit, &QLineEdit::returnPressed,
            this, &DependencyGraphWidget::searchNext);
    connect(m_nextButton, &QPushButton::clicked,
            this, &DependencyGraphWidget::searchNext);
    connect(fitButton, &QPushButton::clicked, this, &DependencyGraphWidget::showAll);
    connect(resetButton, &QPushButton::clicked, this, &DependencyGraphWidget::resetView);
    connect(m_view, &DependencyGraphView::escapePressed, this, [this] {
        clearSearch();
        m_scene->clearHighlight();
    });
    connect(m_view, &DependencyGraphView::emptyAreaPressed,
            m_scene, &DependencyGraphScene::clearHighlight);
}

QString DependencyGraphWidget::buildLegendHtml() const
{
    QStringList parts;
    for (const QPair<QString, QColor> &style : DependencyGraphScene::statusStyles()) {
        parts.append(QStringLiteral("<span style=\"color:%1;\">&#9632; %2</span>")
                         .arg(style.second.name(), style.first));
    }
    QString html = parts.join(QStringLiteral("&nbsp;&nbsp;"));
    html += QStringLiteral("&nbsp;&nbsp;|&nbsp;&nbsp;");
    const QStringList priorities{ QStringLiteral("High"), QStringLiteral("Medium"),
                                  QStringLiteral("Low") };
    for (const QString &priority : priorities) {
        html += QStringLiteral("<span style=\"background-color:%1;\">&nbsp;&nbsp;&nbsp;</span> %2"
                               "&nbsp;&nbsp;")
                    .arg(DependencyGraphScene::fillColorForPriority(priority).name(), priority);
    }
    return html;
}

void DependencyGraphWidget::placeLegend()
{
    if (!m_legend || !m_view)
        return;
    m_legend->adjustSize();
    m_legend->move(8, m_view->height() - m_legend->height() - 8);
    m_legend->raise();
}

bool DependencyGraphWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_view && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
        placeLegend();
    return QWidget::eventFilter(watched, event);
}

void DependencyGraphWidget::setRequirements(const QVector<Requirement> &requirements,
                                            const QVector<RequirementsValidator::Issue> &issues)
{
    clearSearch();
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
    placeLegend();
}

QString DependencyGraphWidget::counterText() const
{
    return m_matchCounter ? m_matchCounter->text() : QString();
}

void DependencyGraphWidget::clearMatchMarks()
{
    for (DependencyGraphNodeItem *item : m_scene->nodeItems())
        item->setSearchState(false, false);
}

void DependencyGraphWidget::runSearch(const QString &text)
{
    clearMatchMarks();
    m_matches = m_scene->nodesMatching(text);
    m_matchIndex = 0;
    for (DependencyGraphNodeItem *item : m_matches)
        item->setSearchState(true, false);

    if (text.trimmed().isEmpty()) {
        m_matchCounter->clear();
        return;
    }
    m_matchIndex = m_matches.isEmpty() ? 0 : 1;
    if (m_matches.isEmpty())
        m_matchCounter->setText(QStringLiteral("0 / 0"));
    else
        jumpToMatch();
}

void DependencyGraphWidget::searchNext()
{
    if (m_matches.isEmpty()) {
        runSearch(m_searchEdit->text());
        return;
    }
    m_matchIndex = (m_matchIndex % m_matches.size()) + 1;
    jumpToMatch();
}

void DependencyGraphWidget::jumpToMatch()
{
    if (m_matches.isEmpty() || m_matchIndex < 1)
        return;
    const int index = qMin(m_matchIndex, m_matches.size()) - 1;
    for (int i = 0; i < m_matches.size(); ++i)
        m_matches.at(i)->setSearchState(true, i == index);

    DependencyGraphNodeItem *item = m_matches.at(index);
    m_matchCounter->setText(QStringLiteral("%1 / %2").arg(index + 1).arg(m_matches.size()));
    // Centre, and zoom in when the whole-graph fit is too far out to read a
    // node at: a match that is centred but 20% wide is not "in view".
    m_view->setAutoFit(false);
    m_view->ensureVisible(item, 60, 60);
    if (m_view->transform().m11() < 0.6)
        m_view->resetTransform();
    m_view->centerOn(item->scenePos() + item->boundingRect().center());
}

void DependencyGraphWidget::clearSearch()
{
    m_searchEdit->blockSignals(true);
    m_searchEdit->clear();
    m_searchEdit->blockSignals(false);
    clearMatchMarks();
    m_matches.clear();
    m_matchIndex = 0;
    m_matchCounter->clear();
}

void DependencyGraphWidget::showAll()
{
    m_scene->clearHighlight();
    m_view->fitAll();
}

void DependencyGraphWidget::resetView()
{
    clearSearch();
    m_scene->clearHighlight();
    m_view->resetZoom();
}

} // namespace Daqster
