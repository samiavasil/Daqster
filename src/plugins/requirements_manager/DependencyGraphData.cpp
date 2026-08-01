#include "DependencyGraphData.h"

#include <algorithm>

namespace Daqster {

namespace {

constexpr int kHorizontalSpacing = 260; //!< px between adjacent layers
constexpr int kVerticalSpacing = 110;   //!< px between nodes within a layer

} // namespace

DependencyGraphData DependencyGraphData::build(const QVector<Requirement> &requirements)
{
    DependencyGraphData data;
    data.m_requirements = requirements;

    const int n = requirements.size();
    data.m_layer.fill(-1, n);

    // One node per requirement (indexes stay aligned with the source vector).
    for (int i = 0; i < n; ++i) {
        const Requirement &req = requirements.at(i);
        GraphNode node;
        node.reqIndex = i;
        node.id = req.id;
        node.title = req.title;
        node.status = req.status;
        node.priority = req.priority;
        node.section = req.section;
        data.m_nodes.append(node);
    }

    // Resolve a reference to a requirement index (case-insensitive, mirrors
    // RequirementsModel::indexOfId) or record it as dangling when it does not
    // exist in the current set.
    auto resolve = [&data](const QString &ref) -> int {
        if (ref.trimmed().isEmpty())
            return -1;
        for (int i = 0; i < data.m_requirements.size(); ++i) {
            if (QString::compare(data.m_requirements.at(i).id, ref, Qt::CaseInsensitive) == 0)
                return i;
        }
        data.m_danglingIds.append(ref);
        return -1;
    };

    // Parent edges (req -> parentId) and dependency edges (req -> dep).
    for (int i = 0; i < n; ++i) {
        const Requirement &req = requirements.at(i);

        const int parentIdx = resolve(req.parentId);
        if (parentIdx >= 0 && parentIdx != i) {
            GraphEdge edge;
            edge.from = i;
            edge.to = parentIdx;
            edge.kind = GraphEdge::Parent;
            data.m_edges.append(edge);
        }

        for (const QString &dep : req.dependencies) {
            const int depIdx = resolve(dep);
            if (depIdx >= 0 && depIdx != i) {
                GraphEdge edge;
                edge.from = i;
                edge.to = depIdx;
                edge.kind = GraphEdge::Dependency;
                data.m_edges.append(edge);
            }
        }
    }

    // Kahn's algorithm over the dependency edges. Nodes with in-degree 0 (no
    // requirement depends on them) seed layer 0; a dependency u -> v means
    // "u depends on v", so u is always laid out to the left of v.
    QVector<QVector<int>> outgoing(n);
    QVector<int> indegree(n, 0);
    for (const GraphEdge &edge : data.m_edges) {
        if (edge.kind != GraphEdge::Dependency)
            continue;
        outgoing[edge.from].append(edge.to);
        ++indegree[edge.to];
    }

    QVector<int> frontier;
    for (int i = 0; i < n; ++i) {
        if (indegree.at(i) == 0)
            frontier.append(i);
    }

    int processed = 0;
    int nextLayer = 0;
    while (!frontier.isEmpty()) {
        const QVector<int> current = frontier;
        frontier.clear();
        for (int u : current) {
            // Seen-set guard: never assign a node twice. Guarantees the pass
            // terminates even on malformed input.
            if (data.m_layer.at(u) != -1)
                continue;
            data.m_layer[u] = nextLayer;
            ++processed;
            for (int v : outgoing.at(u)) {
                --indegree[v];
                if (indegree[v] == 0)
                    frontier.append(v);
            }
        }
        ++nextLayer;
    }

    // Nodes that still have no layer are members of a dependency cycle. Put
    // them all in one residual layer below the DAG layers.
    if (processed < n) {
        data.m_hasCycle = true;
        for (int i = 0; i < n; ++i) {
            if (data.m_layer.at(i) == -1)
                data.m_layer[i] = nextLayer; // residual layer
        }
        ++nextLayer;
    }

    // Assign positions: x by layer, y by id-sorted row within the layer so the
    // residual (cycle) layer is keyed deterministically by id too.
    QVector<QVector<int>> byLayer(nextLayer);
    for (int i = 0; i < n; ++i) {
        const int layer = data.m_layer.at(i);
        if (layer >= 0 && layer < byLayer.size())
            byLayer[layer].append(i);
    }
    for (QVector<int> &members : byLayer) {
        std::sort(members.begin(), members.end(), [&requirements](int a, int b) {
            return QString::compare(requirements.at(a).id, requirements.at(b).id,
                                    Qt::CaseInsensitive) < 0;
        });
    }
    for (int layer = 0; layer < byLayer.size(); ++layer) {
        for (int row = 0; row < byLayer.at(layer).size(); ++row) {
            const int reqIndex = byLayer.at(layer).at(row);
            data.m_nodes[reqIndex].pos =
                QPointF(layer * kHorizontalSpacing, row * kVerticalSpacing);
        }
    }

    return data;
}

QPointF DependencyGraphData::positionFor(int reqIndex) const
{
    if (reqIndex < 0 || reqIndex >= m_nodes.size())
        return QPointF();
    return m_nodes.at(reqIndex).pos;
}

int DependencyGraphData::layerFor(int reqIndex) const
{
    if (reqIndex < 0 || reqIndex >= m_layer.size())
        return -1;
    return m_layer.at(reqIndex);
}

} // namespace Daqster
