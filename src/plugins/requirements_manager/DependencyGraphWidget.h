#pragma once

#include <QGraphicsObject>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHash>
#include <QMouseEvent>
#include <QPair>
#include <QStringList>
#include <QVector>
#include <QWidget>
#include "DependencyGraphData.h"
#include "RequirementsParser.h"
#include "RequirementsValidator.h"

class QLabel;
class QLineEdit;
class QPushButton;

namespace Daqster {

class DependencyGraphNodeItem;
class DependencyGraphEdgeItem;

/**
 * @brief QGraphicsScene subclass hosting the requirement dependency graph.
 *
 * Renders one movable rounded-rect node per requirement plus one arrowed edge
 * per resolved "Родител:" (dashed) / "Зависи от:" (solid) reference. Edges are
 * live: each node's positionChanged() signal drives the incident edges'
 * updateGeometry(), so dragging a node moves its edges with it.
 *
 * Interaction contract (REQ-SW-PL-001, user item 6):
 *  - a **single** click selects the node: it gets a halo, its incident edges
 *    are highlighted, every other node/edge is dimmed and the direct
 *    neighbours keep a medium emphasis;
 *  - a **double** click navigates (navigateRequested);
 *  - Escape or a click on empty space clears the selection;
 *  - a search match is marked with a dashed accent ring.
 */
class DependencyGraphScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit DependencyGraphScene(QObject *parent = nullptr);

    /**
     * @brief Rebuilds the scene from parsed requirements.
     *
     * @param requirements Parsed requirements (one node each).
     * @param issues Validation issues; kept per requirement ID so the node
     *        tooltip can list them.
     */
    void setRequirements(const QVector<Requirement> &requirements,
                         const QVector<RequirementsValidator::Issue> &issues);

    int nodeCount() const { return m_nodeItems.size(); }
    int edgeCount() const { return m_edgeItems.size(); }
    int danglingCount() const { return m_data.danglingCount(); }

    /**
     * @brief Find a node item by (id, repo) composite key. When repo is empty
     *        only the id is compared (backwards compatible).
     */
    DependencyGraphNodeItem *nodeItemForId(const QString &id,
                                           const QString &repo = QString()) const;

    /** @brief Scene edges (a DependencyGraphEdgeItem per GraphEdge). */
    const QVector<DependencyGraphEdgeItem *> &edgeItems() const { return m_edgeItems; }

    /** @brief All node items, in build order. */
    const QVector<DependencyGraphNodeItem *> &nodeItems() const { return m_nodeItems; }

    /**
     * @brief Nodes whose ID or title contains @p text (case-insensitive).
     *
     * Empty @p text yields no matches (an empty query is not "select all").
     */
    QVector<DependencyGraphNodeItem *> nodesMatching(const QString &text) const;

    /**
     * @brief Validation issues recorded for a requirement ID.
     * @return Human readable lines; empty when the requirement is clean.
     */
    QStringList issuesFor(const QString &id) const;

    /**
     * @brief Selects @p node: halo on it, incident edges highlighted, the rest
     *        dimmed, direct neighbours at medium emphasis.
     *
     * @param node Node to select, or nullptr (same as clearHighlight()).
     */
    void selectNode(DependencyGraphNodeItem *node);

    /** @brief Clears the selection emphasis (all items back to Normal). */
    void clearHighlight();

    /** @brief Currently selected node, or nullptr. */
    DependencyGraphNodeItem *selectedNode() const { return m_selected; }

    /**
     * @brief Status border colors (hex-only constants, Qt5/Qt6 safe).
     *
     * The single table for the five dependency statuses of the shared
     * vocabulary (RequirementsValidator::statusVocabulary()).
     */
    static QColor borderColorForStatus(const QString &status);

    /**
     * @brief Priority fill colors (hex-only constants, Qt5/Qt6 safe).
     *        High = darker, Medium = mid, Low = lighter.
     */
    static QColor fillColorForPriority(const QString &priority);

    /**
     * @brief The status -> colour table, in legend order (5 entries).
     */
    static QVector<QPair<QString, QColor>> statusStyles();

signals:
    void navigateRequested(const QString &id);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private:
    DependencyGraphData m_data;
    QVector<DependencyGraphNodeItem *> m_nodeItems;
    QVector<DependencyGraphEdgeItem *> m_edgeItems;
    QHash<QString, QStringList> m_issuesById;
    DependencyGraphNodeItem *m_selected = nullptr;
};

/**
 * @brief Movable rounded-rect node showing a requirement ID + title.
 */
class DependencyGraphNodeItem : public QGraphicsObject
{
    Q_OBJECT

public:
    /** @brief Emphasis level of a node inside a selection. */
    enum class Emphasis {
        Normal,   //!< no selection: full opacity, plain border
        Selected, //!< clicked node: halo + strong border
        Neighbour,//!< directly connected to the selected node
        Dimmed    //!< unrelated to the selection
    };

    explicit DependencyGraphNodeItem(const GraphNode &node, QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    QString requirementId() const { return m_id; }
    QString repo() const { return m_repo; }
    QString title() const { return m_title; }
    QString status() const { return m_status; }
    QString priority() const { return m_priority; }

    /** @brief Resolved outgoing dependency + parent reference count. */
    int dependencyCount() const { return m_dependencyCount; }
    /** @brief Resolved "Родител:" reference count. */
    int parentCount() const { return m_parentCount; }

    /**
     * @brief Sets the counts and the per-requirement validation issues and
     *        recomposes the tooltip (id, title, status, counts, issues).
     */
    void setDependencyInfo(int dependencyCount, int parentCount,
                           const QStringList &issues);

    Emphasis emphasis() const { return m_emphasis; }
    void setEmphasis(Emphasis emphasis);

    bool searchMatch() const { return m_searchMatch; }
    bool searchCurrent() const { return m_searchCurrent; }
    /**
     * @brief Marks the node as a search result; @p current draws the thicker
     *        emphasis so the user can see which hit is on screen.
     */
    void setSearchState(bool match, bool current);

    /** @brief Tooltip lines (id, title, status, counts, issues). */
    QStringList tooltipLines() const { return m_tooltipLines; }

signals:
    /** @brief Emitted on a single click (select/highlight, no navigation). */
    void selected(const QString &id);
    /** @brief Emitted on a double click (navigate to the requirement). */
    void activated(const QString &id);
    /**
     * @brief Emitted after the item's scene position changed (drag or
     *        setPos). The scene routes this to the incident edges so they
     *        can recompute their geometry.
     */
    void positionChanged();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

private:
    void composeTooltip();

    QString m_id;
    QString m_repo;
    QString m_title;
    QString m_status;
    QString m_priority;
    QRectF m_rect;
    QPointF m_pressScenePos;
    int m_dependencyCount = 0;
    int m_parentCount = 0;
    QStringList m_issues;
    QStringList m_tooltipLines;
    Emphasis m_emphasis = Emphasis::Normal;
    bool m_searchMatch = false;
    bool m_searchCurrent = false;
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
    /** @brief Emphasis level of an edge inside a selection. */
    enum class Emphasis {
        Normal,      //!< no selection
        Highlighted, //!< incident to the selected node: thicker, source colour
        Dimmed       //!< faded because it is unrelated to the selection
    };

    explicit DependencyGraphEdgeItem(DependencyGraphNodeItem *from,
                                     DependencyGraphNodeItem *to,
                                     GraphEdge::Kind kind = GraphEdge::Dependency);

    DependencyGraphNodeItem *fromNode() const { return m_from; }
    DependencyGraphNodeItem *toNode() const { return m_to; }
    GraphEdge::Kind kind() const { return m_kind; }

    /**
     * @brief Recomputes the edge path from the current scene positions of
     *        both endpoint nodes.
     */
    void updateGeometry();

    Emphasis emphasis() const { return m_emphasis; }
    /** @brief Applies the emphasis (pen colour/width + opacity). */
    void setEmphasis(Emphasis emphasis);

private:
    void applyStyle();

    DependencyGraphNodeItem *m_from = nullptr;
    DependencyGraphNodeItem *m_to = nullptr;
    GraphEdge::Kind m_kind = GraphEdge::Dependency;
    Emphasis m_emphasis = Emphasis::Normal;
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
    bool autoFit() const { return m_autoFit; }

    /** @brief Fits the whole scene into the viewport ("Покажи всичко"). */
    void fitAll();
    /** @brief Restores 100% zoom and centres the scene ("Нулирай"). */
    void resetZoom();

signals:
    /** @brief Escape pressed while the view has focus. */
    void escapePressed();
    /** @brief Detected after a press that landed on empty canvas. */
    void emptyAreaPressed();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void maybeFitToScene();
    bool m_autoFit = false;
};

/**
 * @brief Widget combining a search row, the graph view and a status legend.
 *
 * Search row (above the view): "Търси по ID или заглавие" line edit, a
 * "Следващ" button and a "3 / 12" counter. Enter and the button jump to the
 * next match (centred in the viewport), Escape clears the highlight, an empty
 * box is not a query. "Покажи всичко" fits the graph, "Нулирай" restores 100%.
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

    DependencyGraphScene *graphScene() const { return m_scene; }
    DependencyGraphView *graphView() const { return m_view; }
    QLineEdit *searchEdit() const { return m_searchEdit; }
    QLabel *matchCounter() const { return m_matchCounter; }
    QLabel *legendLabel() const { return m_legend; }

    /** @brief Number of nodes matching the current query. */
    int matchCount() const { return m_matches.size(); }
    /** @brief 1-based index of the current match (0 when none). */
    int currentMatchIndex() const { return m_matchIndex; }
    /** @brief The "3 / 12" counter text (empty when the box is empty). */
    QString counterText() const;

public slots:
    /** @brief Recomputes the matches for the current text, marks them, jumps
     *         to the next one. */
    void searchNext();
    /** @brief Clears the query, the match marks and the counter. */
    void clearSearch();
    /** @brief Fits the whole graph into the viewport. */
    void showAll();
    /** @brief Restores 100% zoom and clears search + selection. */
    void resetView();

signals:
    void navigateRequested(const QString &id);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void runSearch(const QString &text);
    void jumpToMatch();
    void clearMatchMarks();
    void placeLegend();
    QString buildLegendHtml() const;

    DependencyGraphScene *m_scene;
    DependencyGraphView *m_view;
    QLineEdit *m_searchEdit;
    QPushButton *m_nextButton;
    QLabel *m_matchCounter;
    QLabel *m_legend;
    QLabel *m_warningLabel;
    QVector<DependencyGraphNodeItem *> m_matches;
    int m_matchIndex = 0;
};

} // namespace Daqster
