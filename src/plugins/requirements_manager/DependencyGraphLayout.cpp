#include "DependencyGraphLayout.h"

#include <QPair>

#include <algorithm>

namespace Daqster {

namespace {

// Spacing constants moved here from DependencyGraphData.cpp so the layout owns
// the coordinate policy (DependencyGraphData just calls into it).
constexpr qreal kHorizontalSpacing = 260.0; //!< px between adjacent layers
constexpr qreal kVerticalSpacing = 110.0;   //!< px between rows within a layer
constexpr qreal kLayerPadding = 40.0;       //!< extra px after a wide layer
constexpr int kSweepIterations = 5;         //!< barycenter sweep iterations

// Strict total order on requirement IDs: case-insensitive primary key, then
// case-sensitive, then req index. Guarantees a deterministic ordering even for
// IDs that differ only by case (std::sort needs a strict weak ordering).
bool lessById(const QStringList &ids, int a, int b)
{
    const int cmp = QString::compare(ids.at(a), ids.at(b), Qt::CaseInsensitive);
    if (cmp != 0)
        return cmp < 0;
    if (ids.at(a) != ids.at(b))
        return ids.at(a) < ids.at(b);
    return a < b;
}

// Sort a layer's members by ID (case-insensitive) — the deterministic starting
// point for the barycenter pass.
void sortLayerById(QVector<int> &members, const QStringList &ids)
{
    std::stable_sort(members.begin(), members.end(),
                     [&ids](int a, int b) { return lessById(ids, a, b); });
}

// Sort a layer's members by barycenter value, tie-breaking with the ID order.
void sortLayerByBary(QVector<int> &members, const QStringList &ids,
                     const QVector<qreal> &bary)
{
    std::stable_sort(members.begin(), members.end(), [&ids, &bary](int a, int b) {
        if (bary.at(a) != bary.at(b))
            return bary.at(a) < bary.at(b);
        return lessById(ids, a, b);
    });
}

// Average position of a node's neighbours in the adjacent layer. Nodes without
// neighbours keep their current position (no pull from either side).
void computeBarycenters(const QVector<int> &layers, const QVector<GraphEdge> &edges,
                        int layer, int neighborLayer, const QVector<int> &posInLayer,
                        QVector<qreal> *bary, QVector<int> *neighborCount)
{
    for (int v = 0; v < bary->size(); ++v) {
        (*bary)[v] = 0.0;
        (*neighborCount)[v] = 0;
    }
    for (const GraphEdge &edge : edges) {
        const int fromLayer = layers.at(edge.from);
        const int toLayer = layers.at(edge.to);
        if (fromLayer == neighborLayer && toLayer == layer) {
            (*bary)[edge.to] += posInLayer.at(edge.from);
            ++(*neighborCount)[edge.to];
        } else if (toLayer == neighborLayer && fromLayer == layer) {
            (*bary)[edge.from] += posInLayer.at(edge.to);
            ++(*neighborCount)[edge.from];
        }
    }
    for (int v = 0; v < bary->size(); ++v) {
        if ((*neighborCount)[v] > 0)
            (*bary)[v] /= (*neighborCount)[v];
        else
            (*bary)[v] = posInLayer.at(v); // keep position
    }
}

// One left→right sweep: reorder layer i by the barycenter of its neighbours
// in layer i-1 (which was already reordered earlier in this pass).
void sweepLeftRight(const QVector<int> &layers, const QVector<GraphEdge> &edges,
                    const QStringList &ids, QVector<QVector<int>> *order,
                    QVector<int> *posInLayer)
{
    const int layerCount = order->size();
    QVector<qreal> bary(ids.size(), 0.0);
    QVector<int> neighborCount(ids.size(), 0);
    for (int layer = 1; layer < layerCount; ++layer) {
        computeBarycenters(layers, edges, layer, layer - 1, *posInLayer,
                           &bary, &neighborCount);
        sortLayerByBary((*order)[layer], ids, bary);
        for (int row = 0; row < (*order)[layer].size(); ++row)
            (*posInLayer)[(*order)[layer].at(row)] = row;
    }
}

// One right→left sweep: reorder layer i by the barycenter of its neighbours
// in layer i+1 (already reordered earlier in this pass).
void sweepRightLeft(const QVector<int> &layers, const QVector<GraphEdge> &edges,
                    const QStringList &ids, QVector<QVector<int>> *order,
                    QVector<int> *posInLayer)
{
    const int layerCount = order->size();
    QVector<qreal> bary(ids.size(), 0.0);
    QVector<int> neighborCount(ids.size(), 0);
    for (int layer = layerCount - 2; layer >= 0; --layer) {
        computeBarycenters(layers, edges, layer, layer + 1, *posInLayer,
                           &bary, &neighborCount);
        sortLayerByBary((*order)[layer], ids, bary);
        for (int row = 0; row < (*order)[layer].size(); ++row)
            (*posInLayer)[(*order)[layer].at(row)] = row;
    }
}

} // namespace

QVector<QVector<int>> DependencyGraphLayout::orderLayers(int nodeCount,
                                                         const QVector<GraphEdge> &edges,
                                                         const QVector<int> &layers,
                                                         const QStringList &ids)
{
    const int layerCount = layers.isEmpty()
        ? 0
        : (*std::max_element(layers.constBegin(), layers.constEnd())) + 1;

    // Initial order: ID-sorted per layer (matches the pre-Sugiyama layout).
    QVector<QVector<int>> order(layerCount);
    for (int v = 0; v < nodeCount; ++v) {
        const int layer = layers.at(v);
        if (layer >= 0 && layer < layerCount)
            order[layer].append(v);
    }
    for (QVector<int> &members : order)
        sortLayerById(members, ids);

    QVector<int> posInLayer(nodeCount, -1);
    for (int layer = 0; layer < layerCount; ++layer) {
        for (int row = 0; row < order.at(layer).size(); ++row)
            posInLayer[order.at(layer).at(row)] = row;
    }

    // Barycenter sweeps; keep the ordering with the fewest crossings.
    QVector<QVector<int>> best = order;
    int bestCrossings = crossingCount(nodeCount, edges, best);
    for (int iteration = 0; iteration < kSweepIterations; ++iteration) {
        sweepLeftRight(layers, edges, ids, &order, &posInLayer);
        sweepRightLeft(layers, edges, ids, &order, &posInLayer);
        const int crossings = crossingCount(nodeCount, edges, order);
        if (crossings < bestCrossings) {
            bestCrossings = crossings;
            best = order;
        }
        if (bestCrossings == 0)
            break; // cannot improve further
    }
    return best;
}

void DependencyGraphLayout::assignCoordinates(const QVector<QVector<int>> &orderedLayers,
                                              const QVector<qreal> &nodeWidths,
                                              const QStringList &ids,
                                              QVector<QPointF> *outPositions)
{
    Q_UNUSED(ids);
    if (!outPositions)
        return;

    const int layerCount = orderedLayers.size();

    // Horizontal positions: default per-layer gap, widened when a layer holds
    // nodes wider than the gap so long titles never overlap.
    QVector<qreal> layerX(layerCount, 0.0);
    qreal cursor = 0.0;
    for (int layer = 0; layer < layerCount; ++layer) {
        layerX[layer] = cursor;
        qreal maxWidth = 0.0;
        if (!nodeWidths.isEmpty()) {
            for (int v : orderedLayers.at(layer))
                maxWidth = qMax(maxWidth, nodeWidths.at(v));
        }
        const qreal step = (maxWidth <= 0.0)
            ? kHorizontalSpacing
            : qMax(kHorizontalSpacing, maxWidth + kLayerPadding);
        cursor += step;
    }

    // Vertical positions: rows spaced by kVerticalSpacing, each layer centred
    // around the tallest layer's middle so the offset is never negative.
    qreal maxLayerHeight = 0.0;
    for (int layer = 0; layer < layerCount; ++layer)
        maxLayerHeight = qMax(maxLayerHeight,
                              orderedLayers.at(layer).size() * kVerticalSpacing);
    for (int layer = 0; layer < layerCount; ++layer) {
        const qreal layerHeight = orderedLayers.at(layer).size() * kVerticalSpacing;
        const qreal yOffset = (maxLayerHeight - layerHeight) / 2.0;
        const QVector<int> &members = orderedLayers.at(layer);
        for (int row = 0; row < members.size(); ++row) {
            (*outPositions)[members.at(row)] =
                QPointF(layerX.at(layer), row * kVerticalSpacing + yOffset);
        }
    }
}

int DependencyGraphLayout::crossingCount(int nodeCount,
                                         const QVector<GraphEdge> &edges,
                                         const QVector<QVector<int>> &orderedLayers)
{
    QVector<int> layerOf(nodeCount, -1);
    QVector<int> pos(nodeCount, -1);
    for (int layer = 0; layer < orderedLayers.size(); ++layer) {
        const QVector<int> &members = orderedLayers.at(layer);
        for (int row = 0; row < members.size(); ++row) {
            layerOf[members.at(row)] = layer;
            pos[members.at(row)] = row;
        }
    }

    int crossings = 0;
    for (int layer = 0; layer + 1 < orderedLayers.size(); ++layer) {
        // Normalize every edge between this layer and the next to a
        // (leftPos, rightPos) span so fan-in/fan-out edges share an endpoint
        // and never count as crossing.
        QVector<QPair<int, int>> spans;
        for (const GraphEdge &edge : edges) {
            const int a = edge.from;
            const int b = edge.to;
            if (layerOf.at(a) == layer && layerOf.at(b) == layer + 1)
                spans.append(qMakePair(pos.at(a), pos.at(b)));
            else if (layerOf.at(b) == layer && layerOf.at(a) == layer + 1)
                spans.append(qMakePair(pos.at(b), pos.at(a)));
        }
        std::stable_sort(spans.begin(), spans.end());

        // Count inversions in the right positions with a Fenwick tree:
        // process right→left and count how many already-seen rights are
        // strictly smaller than the current one.
        QVector<int> bit(pos.size() + 1, 0);
        auto update = [&bit](int idx, int delta) {
            for (++idx; idx < static_cast<int>(bit.size()); idx += idx & -idx)
                bit[idx] += delta;
        };
        auto query = [&bit](int idx) {
            int sum = 0;
            for (++idx; idx > 0; idx -= idx & -idx)
                sum += bit[idx];
            return sum;
        };
        for (int i = static_cast<int>(spans.size()) - 1; i >= 0; --i) {
            const int right = spans.at(i).second;
            crossings += query(right - 1);
            update(right, 1);
        }
    }
    return crossings;
}

} // namespace Daqster
