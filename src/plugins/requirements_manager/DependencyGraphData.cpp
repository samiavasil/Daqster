#include "DependencyGraphData.h"

#include "DependencyGraphLayout.h"

namespace Daqster {

DependencyGraphData DependencyGraphData::build(const QVector<Requirement> &requirements)
{
    return build(requirements, QVector<Idea>());
}

DependencyGraphData DependencyGraphData::build(const QVector<Requirement> &requirements,
                                               const QVector<Idea> &ideas)
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
        node.repo = req.repo;
        node.title = req.title;
        node.status = req.status;
        node.priority = req.priority;
        node.section = req.section;
        node.width = qMax(120.0, 36.0 + req.title.size() * 6.5);
        data.m_nodes.append(node);
    }

    // Resolve a reference to a requirement index (case-insensitive, mirrors
    // RequirementsModel::indexOfId) or record it as dangling when it does not
    // exist in the current set. With a merged multi-repo vector a bare ID can
    // resolve to several requirements (duplicate IDs across repos are flagged
    // as Errors by the validator); when that happens the reference coming from
    // the same repo wins, so cross-tree edges still resolve deterministically.
    auto resolve = [&data](const QString &ref, const QString &fromRepo) -> int {
        if (ref.trimmed().isEmpty())
            return -1;
        int firstMatch = -1;
        int sameRepoMatch = -1;
        for (int i = 0; i < data.m_requirements.size(); ++i) {
            const Requirement &req = data.m_requirements.at(i);
            if (QString::compare(req.id, ref, Qt::CaseInsensitive) != 0)
                continue;
            if (firstMatch < 0)
                firstMatch = i;
            if (!fromRepo.isEmpty()
                && QString::compare(req.repo, fromRepo, Qt::CaseInsensitive) == 0) {
                sameRepoMatch = i;
                break;
            }
        }
        if (sameRepoMatch >= 0)
            return sameRepoMatch;
        if (firstMatch >= 0)
            return firstMatch;
        data.m_danglingIds.append(ref);
        return -1;
    };

    // Parent edges (req -> parentId) and dependency edges (req -> dep).
    for (int i = 0; i < n; ++i) {
        const Requirement &req = requirements.at(i);

        // "Родител:" may list several IDs; each one is a parent edge. Records
        // built in memory (callers/tests) may carry only parentId.
        QStringList parents = req.parentIds;
        if (parents.isEmpty() && !req.parentId.trimmed().isEmpty())
            parents.append(req.parentId);
        for (const QString &parentId : parents) {
            const int parentIdx = resolve(parentId, req.repo);
            if (parentIdx < 0 || parentIdx == i)
                continue;
            GraphEdge edge;
            edge.from = i;
            edge.to = parentIdx;
            edge.kind = GraphEdge::Parent;
            data.m_edges.append(edge);
        }

        for (const QString &dep : req.dependencies) {
            const int depIdx = resolve(dep, req.repo);
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

    // ── Идеите като пунктирани възли (REQ-SW-PL-098) ──────────────────────
    // Only a promoted idea whose requirement exists becomes a node: an idea that
    // was never promoted has nothing to connect to, and a promotion naming a
    // missing requirement is a validator finding (idea-link), not a second graph
    // warning. The requirement's absence is therefore skipped SILENTLY here.
    for (int i = 0; i < ideas.size(); ++i) {
        const Idea &idea = ideas.at(i);
        const QString promoted = idea.promotedId.trimmed();
        if (promoted.isEmpty())
            continue;
        int reqIdx = -1;
        for (int j = 0; j < n; ++j) {
            if (QString::compare(requirements.at(j).id, promoted, Qt::CaseInsensitive) == 0) {
                reqIdx = j;
                break;
            }
        }
        if (reqIdx < 0)
            continue;

        GraphNode node;
        node.reqIndex = -1;
        node.ideaIndex = i;
        node.kind = GraphNode::Idea;
        node.id = idea.id;
        node.title = idea.title;
        node.width = qMax(120.0, 36.0 + idea.title.size() * 6.5);
        data.m_nodes.append(node);

        GraphEdge edge;
        edge.from = data.m_nodes.size() - 1;
        edge.to = reqIdx;
        edge.kind = GraphEdge::IdeaPromotion;
        data.m_edges.append(edge);
    }

    // Sugiyama phases 2 & 3 (layout): barycenter crossing minimization (all
    // edges — Parent, Dependency AND IdeaPromotion — take part in the ordering)
    // and coordinate assignment with size-aware horizontal spacing and vertically
    // centred layers. Phase 1 (Kahn + residual layer, above) is untouched, so
    // layerFor() values are identical to the pre-Sugiyama layout.
    //
    // Idea nodes join the layout: each one sits in the layer of the requirement
    // it points at, so the graph reads "idea -> requirement" left to right
    // (REQ-SW-PL-098). m_layer keeps requirement indexes only, because
    // layerFor() is a requirement lookup.
    const int total = data.m_nodes.size();
    QVector<int> layers = data.m_layer;
    layers.reserve(total);
    for (int i = n; i < total; ++i) {
        int layer = 0;
        for (const GraphEdge &edge : data.m_edges) {
            if (edge.kind != GraphEdge::IdeaPromotion || edge.from != i)
                continue;
            if (edge.to >= 0 && edge.to < data.m_layer.size())
                layer = data.m_layer.at(edge.to);
        }
        layers.append(layer);
    }

    QStringList ids;
    for (const GraphNode &node : data.m_nodes)
        ids.append(node.id);

    const QVector<QVector<int>> orderedLayers =
        DependencyGraphLayout::orderLayers(total, data.m_edges, layers, ids);

    QVector<qreal> nodeWidths(total, 0.0);
    for (int i = 0; i < total; ++i)
        nodeWidths[i] = data.m_nodes.at(i).width;

    QVector<QPointF> positions(total);
    DependencyGraphLayout::assignCoordinates(orderedLayers, nodeWidths, ids, &positions);
    for (int i = 0; i < total; ++i)
        data.m_nodes[i].pos = positions.at(i);

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
