#pragma once

#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>
#include "RequirementsParser.h"

namespace Daqster {

/**
 * @brief Layout node for the interactive dependency graph.
 *
 * One GraphNode is produced per parsed requirement. @c pos is filled in by
 * DependencyGraphData::build() from the layered layout.
 */
struct GraphNode
{
    int reqIndex = -1; //!< index into the source QVector<Requirement>
    QString id;        //!< e.g. "REQ-SW-PL-001"
    QString title;
    QString status;    //!< ACTIVE | DONE | CANCELLED
    QString priority;  //!< High | Medium | Low
    QString section;   //!< "active" | "archive"
    QPointF pos;       //!< computed scene position of the node
    qreal width = 120.0; //!< visual width in px (same formula as the widget)
};

/**
 * @brief Directed edge between two graph nodes.
 *
 * kind == Parent:     "Родител:" relationship (rendered dashed)
 * kind == Dependency: "Зависи от:" relationship (rendered solid)
 *
 * @c danglingId is always empty for stored edges — unresolvable references are
 * recorded on DependencyGraphData::danglingIds() instead of becoming edges.
 */
struct GraphEdge
{
    int from = -1; //!< source graph node index (child / dependent requirement)
    int to = -1;   //!< target graph node index (parent / dependency requirement)
    enum Kind {
        Parent,
        Dependency
    } kind = Dependency;
    QString danglingId; //!< always empty for stored edges
};

/**
 * @brief Pure-QtCore graph construction + layered layout for requirements.
 *
 * build() produces one node per requirement plus two edge families:
 *   - Parent edges:     req -> parentId (when the parent resolves)
 *   - Dependency edges: req -> dep      (when the dependency resolves)
 *
 * Reference lookup is case-insensitive (mirrors RequirementsModel::indexOfId).
 * Unresolvable references are collected into danglingIds() and never become
 * edges.
 *
 * The layout runs Kahn's algorithm over the dependency edges. Nodes that
 * remain after the topological pass belong to a dependency cycle and are
 * placed in a residual layer (hasCycle() == true). A seen-set guard (never
 * re-process a node already assigned to a layer) guarantees the pass always
 * terminates, mirroring RequirementsModel::resolveHierarchyParent's cycle
 * guard.
 *
 * No Q_OBJECT — QtCore only, so the class compiles directly into the headless
 * unit test binary.
 */
class DependencyGraphData
{
public:
    static DependencyGraphData build(const QVector<Requirement> &requirements);

    const QVector<GraphNode> &nodes() const { return m_nodes; }
    const QVector<GraphEdge> &edges() const { return m_edges; }

    /**
     * @brief Scene position for a source requirement index.
     * @return (0,0) when reqIndex is out of range.
     */
    QPointF positionFor(int reqIndex) const;

    /**
     * @brief Layered level for a source requirement index (0 = leftmost).
     * @return -1 when reqIndex is out of range.
     */
    int layerFor(int reqIndex) const;

    /** @brief true when the dependency graph contains a cycle. */
    bool hasCycle() const { return m_hasCycle; }

    /** @brief Number of unresolved "Родител:" / "Зависи от:" references. */
    int danglingCount() const { return m_danglingIds.size(); }

    /** @brief Unresolved reference IDs (one entry per occurrence). */
    QStringList danglingIds() const { return m_danglingIds; }

private:
    QVector<Requirement> m_requirements;
    QVector<GraphNode> m_nodes;
    QVector<GraphEdge> m_edges;
    QVector<int> m_layer; //!< per source req index; -1 = not laid out
    bool m_hasCycle = false;
    QStringList m_danglingIds;
};

} // namespace Daqster
