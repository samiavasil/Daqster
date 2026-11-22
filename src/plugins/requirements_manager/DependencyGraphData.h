#pragma once

#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>
#include "IdeaReader.h"
#include "RequirementsParser.h"

namespace Daqster {

/**
 * @brief Layout node for the interactive dependency graph.
 *
 * One GraphNode is produced per parsed requirement, plus one per promoted idea
 * (REQ-SW-PL-098) when the caller supplied the idea register. @c pos is filled
 * in by DependencyGraphData::build() from the layered layout.
 */
struct GraphNode
{
    enum Kind {
        Requirement, //!< a requirement (solid node, status colour)
        Idea         //!< a promoted idea (dashed node, no status — REQ-SW-PL-098)
    };

    int reqIndex = -1; //!< index into the source QVector<Requirement>; -1 for ideas
    int ideaIndex = -1; //!< index into the source QVector<Idea>; -1 for requirements
    Kind kind = Requirement;
    QString id;        //!< e.g. "REQ-SW-PL-001" or "IDEA-002"
    QString repo;      //!< "public" | "private" | "other" (REQ-SW-PL-012)
    QString title;
    QString status;    //!< ACTIVE | DONE | CANCELLED (empty for an idea)
    QString priority;  //!< High | Medium | Low (empty for an idea)
    QString section;   //!< "active" | "archive" (empty for an idea)
    QPointF pos;       //!< computed scene position of the node
    qreal width = 120.0; //!< visual width in px (same formula as the widget)
};

/**
 * @brief Directed edge between two graph nodes.
 *
 * kind == Parent:        "Родител:" relationship (rendered dashed)
 * kind == Dependency:    "Зависи от:" relationship (rendered solid)
 * kind == IdeaPromotion: idea -> the requirement it became (own dashed style,
 *                        REQ-SW-PL-098), so the two dashed families differ
 *
 * @c danglingId is always empty for stored edges — unresolvable references are
 * recorded on DependencyGraphData::danglingIds() instead of becoming edges.
 */
struct GraphEdge
{
    int from = -1; //!< source graph node index (child / dependent / idea)
    int to = -1;   //!< target graph node index (parent / dependency / requirement)
    enum Kind {
        Parent,
        Dependency,
        IdeaPromotion
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

    /**
     * @brief Builds the graph and adds the promoted ideas as dashed nodes.
     *
     * Only ideas whose header names a requirement that exists in @p requirements
     * become nodes: an idea that was never promoted has nothing to connect to,
     * and an unpromoted idea is a normal state, not a broken one (the missing
     * requirement is reported by RequirementsValidator::validateIdeaLinks()).
     * Each such idea gets one IdeaPromotion edge to its requirement, and sits in
     * the requirement's layer so the picture reads "idea -> requirement".
     *
     * @param requirements Parsed requirements (solid nodes).
     * @param ideas The idea register (dashed nodes; empty keeps the old graph).
     */
    static DependencyGraphData build(const QVector<Requirement> &requirements,
                                     const QVector<Idea> &ideas);

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
