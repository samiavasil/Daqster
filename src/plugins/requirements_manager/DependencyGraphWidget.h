#pragma once

#include <QGraphicsObject>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QWidget>
#include <QVector>
#include "DependencyGraphData.h"
#include "RequirementsParser.h"
#include "RequirementsValidator.h"

class QLabel;

namespace Daqster {

class DependencyGraphNodeItem;
class DependencyGraphEdgeItem;

/**
 * @brief QGraphicsScene subclass hosting the requirement dependency graph.
 *
 * Renders one movable rounded-rect node per requirement plus one arrowed edge
 * per resolved "Родител:" (dashed) / "Зависи от:" (solid) reference. Clicking
 * a node emits navigateRequested(id). Edges are live: each node's
 * positionChanged() signal drives the incident edges' updateGeometry(), so
 * dragging a node moves its edges with it.
 */
class DependencyGraphScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit DependencyGraphScene(QObject *parent = nullptr);

    /**
     * @brief Rebuilds the scene from parsed requirements.
     *
     * @param issues validation report (reserved for future per-node
     *        highlighting; the dangling warning uses the graph's own count).
     */
    void setRequirements(const QVector<Requirement> &requirements,
                         const QVector<RequirementsValidator::Issue> &issues);

    int nodeCount() const { return m_nodeItems.size(); }
    int edgeCount() const { return m_edgeItems.size(); }
    int danglingCount() const { return m_data.danglingCount(); }

    /** @brief Scene node with the given requirement ID, or nullptr. */
    /**
     * @brief Find a node item by (id, repo) composite key. When repo is empty
     *        only the id is compared (backwards compatible).
     */
    DependencyGraphNodeItem *nodeItemForId(const QString &id,
                                           const QString &repo = QString()) const;

    /** @brief Scene edges (a DependencyGraphEdgeItem per GraphEdge). */
    const QVector<DependencyGraphEdgeItem *> &edgeItems() const { return m_edgeItems; }

    /**
     * @brief Status border colors (hex-only constants, Qt5/Qt6 safe).
     */
    static QColor borderColorForStatus(const QString &status);

    /**
     * @brief Priority fill colors (hex-only constants, Qt5/Qt6 safe).
     *        High = darker, Medium = mid, Low = lighter.
     */
    static QColor fillColorForPriority(const QString &priority);

signals:
    void navigateRequested(const QString &id);

private:
    DependencyGraphData m_data;
    QVector<DependencyGraphNodeItem *> m_nodeItems;
    QVector<DependencyGraphEdgeItem *> m_edgeItems;
};

/**
 * @brief Movable rounded-rect node showing a requirement ID + title.
 */
class DependencyGraphNodeItem : public QGraphicsObject
{
    Q_OBJECT

public:
    explicit DependencyGraphNodeItem(const GraphNode &node, QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    QString requirementId() const { return m_id; }
    QString repo() const { return m_repo; }

signals:
    void clicked(const QString &id);
    /**
     * @brief Emitted after the item's scene position changed (drag or
     *        setPos). The scene routes this to the incident edges so they
     *        can recompute their geometry.
     */
    void positionChanged();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QString m_id;
    QString m_repo;
    QString m_title;
    QString m_status;
    QString m_priority;
    QRectF m_rect;
    QPointF m_pressScenePos;
};

/**
 * @brief Directed arrow edge between two graph nodes.
 *
 * The path is stored in scene coordinates and rebuilt by updateGeometry()
 * whenever either endpoint moves, so edges follow their nodes in real time.
 * The start/end points are shortened to the nodes' actual bounding-rect
 * boundaries (per-node half-sizes, not a hardcoded radius).
 */
class DependencyGraphEdgeItem : public QGraphicsPathItem
{
public:
    explicit DependencyGraphEdgeItem(DependencyGraphNodeItem *from,
                                     DependencyGraphNodeItem *to);

    DependencyGraphNodeItem *fromNode() const { return m_from; }
    DependencyGraphNodeItem *toNode() const { return m_to; }

    /**
     * @brief Recomputes the edge path from the current scene positions of
     *        both endpoint nodes.
     */
    void updateGeometry();

private:
    DependencyGraphNodeItem *m_from = nullptr;
    DependencyGraphNodeItem *m_to = nullptr;
};

/**
 * @brief Zoomable QGraphicsView for the dependency graph.
 *
 * While auto-fit is enabled (set by DependencyGraphWidget::setRequirements),
 * resizeEvent/showEvent refit the scene to the viewport so a graph populated
 * before the window is resized/shown is not stuck at a tiny zoom. Wheel zoom
 * disables auto-fit so the user's zoom is not clobbered.
 */
class DependencyGraphView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit DependencyGraphView(QWidget *parent = nullptr);

    /** @brief Enable/disable automatic fit-to-viewport on resize/show. */
    void setAutoFit(bool enabled);

protected:
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void maybeFitToScene();
    bool m_autoFit = false;
};

/**
 * @brief Widget combining the graph view, a color legend and a dangling
 *        reference warning label.
 */
class DependencyGraphWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DependencyGraphWidget(QWidget *parent = nullptr);

    /**
     * @brief Rebuilds the graph and refreshes the dangling warning label.
     */
    void setRequirements(const QVector<Requirement> &requirements,
                         const QVector<RequirementsValidator::Issue> &issues);

signals:
    void navigateRequested(const QString &id);

private:
    DependencyGraphScene *m_scene;
    DependencyGraphView *m_view;
    QLabel *m_warningLabel;
};

} // namespace Daqster
