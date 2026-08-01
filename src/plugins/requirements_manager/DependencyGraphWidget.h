#pragma once

#include <QGraphicsObject>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QWidget>
#include <QVector>
#include "DependencyGraphData.h"
#include "RequirementsParser.h"
#include "RequirementsValidator.h"

class QLabel;

namespace Daqster {

class DependencyGraphNodeItem;

/**
 * @brief QGraphicsScene subclass hosting the requirement dependency graph.
 *
 * Renders one movable rounded-rect node per requirement plus one arrowed edge
 * per resolved "Родител:" (dashed) / "Зависи от:" (solid) reference. Clicking
 * a node emits navigateRequested(id).
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
    QVector<QGraphicsPathItem *> m_edgeItems;
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

    QString requirementId() const { return m_id; }

signals:
    void clicked(const QString &id);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QString m_id;
    QString m_title;
    QString m_status;
    QString m_priority;
    QRectF m_rect;
    QPointF m_pressScenePos;
};

/**
 * @brief Zoomable QGraphicsView for the dependency graph.
 */
class DependencyGraphView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit DependencyGraphView(QWidget *parent = nullptr);

protected:
    void wheelEvent(QWheelEvent *event) override;
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
