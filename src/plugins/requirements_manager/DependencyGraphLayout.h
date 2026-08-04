#pragma once

#include <QPointF>
#include <QStringList>
#include <QVector>
#include "DependencyGraphData.h"

namespace Daqster {

/**
 * @brief Sugiyama-style layered layout — phases 2 & 3 (ordering + coordinates).
 *
 * Phase 1 (layering) is owned by DependencyGraphData::build(): Kahn's
 * algorithm over the dependency edges plus a cycle-safe residual layer. This
 * class consumes that layering and produces the final scene coordinates:
 *
 *   - orderLayers():   phase 2 — crossing minimization with the barycenter
 *                      heuristic. Starting from a case-insensitive ID-sorted
 *                      order per layer, it sweeps left→right / right→left
 *                      (kSweepIterations iterations), reordering each layer by
 *                      the average position of its neighbours in the adjacent
 *                      layer, and keeps the ordering with the fewest
 *                      crossings (crossingCount()).
 *
 *                      IMPORTANT: ALL edges (Parent AND Dependency) take part
 *                      in the ordering pass. Parent edges may connect
 *                      arbitrary levels; Dependency edges always point from a
 *                      lower to a higher layer, but the ordering pass treats
 *                      the graph as undirected for barycenter purposes. Cycle
 *                      edges live inside the residual layer (same-layer) and
 *                      therefore never contribute to inter-layer crossings.
 *
 *   - assignCoordinates(): phase 3 — turns the ordered layers into scene
 *                      coordinates. x = layer * kHorizontalSpacing, widened
 *                      per layer when the layer holds nodes wider than the gap
 *                      (long titles never overlap), y = row *
 *                      kVerticalSpacing, vertically centred so every layer is
 *                      balanced around the tallest one and no coordinate is
 *                      negative.
 *
 *   - crossingCount(): two-level crossing counter between adjacent layers.
 *                      Edges are normalized left→right, sorted by endpoint
 *                      order and the number of inversions is counted with a
 *                      Fenwick tree — O(E log E), deterministic, correct for
 *                      requirement-scale graphs.
 *
 * Pure QtCore (no Q_OBJECT), so the class compiles directly into the headless
 * unit test binary.
 */
class DependencyGraphLayout
{
public:
    /**
     * @brief Order the nodes of every layer to minimize edge crossings.
     *
     * @param nodeCount number of graph nodes (req indexes 0..nodeCount-1)
     * @param edges     all stored edges (Parent + Dependency)
     * @param layers    per-node layer index (from DependencyGraphData::layerFor)
     * @param ids       per-node requirement ID (deterministic tie-break)
     * @return per-layer node (req index) ordering with the fewest crossings
     *         found; identical to the ID-sorted order when no improvement.
     */
    static QVector<QVector<int>> orderLayers(int nodeCount,
                                             const QVector<GraphEdge> &edges,
                                             const QVector<int> &layers,
                                             const QStringList &ids);

    /**
     * @brief Compute scene positions (node centers) from ordered layers.
     *
     * @param orderedLayers per-layer node (req index) ordering from orderLayers()
     * @param nodeWidths    per-node visual width in px; pass an empty vector
     *                      to disable size-aware horizontal spacing (falls back
     *                      to the fixed layer gap)
     * @param ids           per-node requirement ID (reserved for future use)
     * @param outPositions  receives the QPointF center per req index; must be
     *                      pre-sized to nodeCount. Nodes outside every layer
     *                      keep their existing entry.
     */
    static void assignCoordinates(const QVector<QVector<int>> &orderedLayers,
                                  const QVector<qreal> &nodeWidths,
                                  const QStringList &ids,
                                  QVector<QPointF> *outPositions);

    /**
     * @brief Number of crossings between adjacent layers of an ordering.
     *
     * Only edges whose endpoints lie in two consecutive layers count. Edges
     * sharing an endpoint (fan-in / fan-out) never produce a crossing.
     *
     * @return total crossing count over all adjacent layer pairs.
     */
    static int crossingCount(int nodeCount,
                             const QVector<GraphEdge> &edges,
                             const QVector<QVector<int>> &orderedLayers);
};

} // namespace Daqster
