#include <QtTest>

#include "DependencyGraphData.h"
#include "DependencyGraphLayout.h"
#include "RequirementsParser.h"
#include "test_graph_layout.h"

using namespace Daqster;

namespace {

Requirement makeRequirement(const QString &id, const QString &section,
                            const QString &parentId,
                            const QStringList &dependencies = QStringList())
{
    Requirement req;
    req.id = id;
    req.title = id;
    req.status = QStringLiteral("ACTIVE");
    req.priority = QStringLiteral("Medium");
    req.section = section;
    req.parentId = parentId;
    req.dependencies = dependencies;
    return req;
}

QStringList idsOf(const QVector<Requirement> &reqs)
{
    QStringList ids;
    for (const Requirement &req : reqs)
        ids.append(req.id);
    return ids;
}

// Reconstruct the per-node layer map from a built graph (mirrors what the
// layout consumes: DependencyGraphData::layerFor() per source index).
QVector<int> layersOf(const DependencyGraphData &data, int nodeCount)
{
    QVector<int> layers(nodeCount, -1);
    for (const GraphNode &node : data.nodes())
        layers[node.reqIndex] = data.layerFor(node.reqIndex);
    return layers;
}

} // namespace

void TestGraphLayout::layersPreserved()
{
    // Small fixture: PL-002 and PL-003 both depend on PL-001, so Kahn puts
    // PL-002/PL-003 on layer 0 and PL-001 on layer 1.
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active"),
                                QString(), QStringList()));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active"),
                                QString(),
                                QStringList() << QStringLiteral("REQ-SW-PL-001")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-003"), QStringLiteral("active"),
                                QString(),
                                QStringList() << QStringLiteral("REQ-SW-PL-001")));

    const DependencyGraphData data = DependencyGraphData::build(reqs);
    QCOMPARE(data.layerFor(0), 1);
    QCOMPARE(data.layerFor(1), 0);
    QCOMPARE(data.layerFor(2), 0);

    // orderLayers() must preserve the Kahn layer assignment exactly — it only
    // reorders within layers, it never moves a node across layers.
    const QVector<QVector<int>> ordered =
        DependencyGraphLayout::orderLayers(reqs.size(), data.edges(),
                                           layersOf(data, reqs.size()),
                                           idsOf(reqs));
    QCOMPARE(ordered.size(), 2);
    QCOMPARE(ordered.at(0).size(), 2);
    QVERIFY(ordered.at(0).contains(1)); // PL-002
    QVERIFY(ordered.at(0).contains(2)); // PL-003
    QCOMPARE(ordered.at(1).size(), 1);
    QCOMPARE(ordered.at(1).at(0), 0); // PL-001
}

void TestGraphLayout::crossingsReduced()
{
    // Fixture: A,B in the left layer; C,D in the right layer; edges A→C, A→D,
    // B→C. The ID-sorted order (A,B / C,D) has exactly one crossing (A→D
    // crosses B→C); the barycenter sweep must eliminate it.
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-A"), QStringLiteral("active"),
                                QString(),
                                QStringList() << QStringLiteral("REQ-C")
                                              << QStringLiteral("REQ-D")));
    reqs.append(makeRequirement(QStringLiteral("REQ-B"), QStringLiteral("active"),
                                QString(),
                                QStringList() << QStringLiteral("REQ-C")));
    reqs.append(makeRequirement(QStringLiteral("REQ-C"), QStringLiteral("active"),
                                QString(), QStringList()));
    reqs.append(makeRequirement(QStringLiteral("REQ-D"), QStringLiteral("active"),
                                QString(), QStringList()));

    QVector<GraphEdge> edges;
    GraphEdge edge;
    edge.kind = GraphEdge::Dependency;
    edge.from = 0; edge.to = 2; edges.append(edge); // A→C
    edge.from = 0; edge.to = 3; edges.append(edge); // A→D
    edge.from = 1; edge.to = 2; edges.append(edge); // B→C

    const QVector<int> layers = {0, 0, 1, 1};
    const QStringList ids = idsOf(reqs);

    // 1 crossing for the ID-sorted order, 0 after Sugiyama ordering.
    const QVector<QVector<int>> idOrder = {{0, 1}, {2, 3}};
    QCOMPARE(DependencyGraphLayout::crossingCount(4, edges, idOrder), 1);

    const QVector<QVector<int>> ordered =
        DependencyGraphLayout::orderLayers(4, edges, layers, ids);
    QCOMPARE(DependencyGraphLayout::crossingCount(4, edges, ordered), 0);

    // The full pipeline agrees: D (index 3) moves above C (index 2) in the
    // right layer, so its y is smaller.
    const DependencyGraphData data = DependencyGraphData::build(reqs);
    QVERIFY2(data.positionFor(3).y() < data.positionFor(2).y(),
             "barycenter sweep must put D above C to remove the crossing");
}

void TestGraphLayout::deterministic()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active"),
                                QString(), QStringList()));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active"),
                                QString(),
                                QStringList() << QStringLiteral("REQ-SW-PL-001")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-003"), QStringLiteral("active"),
                                QString(),
                                QStringList() << QStringLiteral("REQ-SW-PL-001")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-004"), QStringLiteral("active"),
                                QString(),
                                QStringList() << QStringLiteral("REQ-SW-PL-002")
                                              << QStringLiteral("REQ-SW-PL-003")));

    // Two identical builds must produce byte-identical positions.
    const DependencyGraphData first = DependencyGraphData::build(reqs);
    const DependencyGraphData second = DependencyGraphData::build(reqs);
    QCOMPARE(first.nodes().size(), second.nodes().size());
    for (const GraphNode &node : first.nodes())
        QCOMPARE(first.positionFor(node.reqIndex), second.positionFor(node.reqIndex));

    // Direct orderLayers() calls are deterministic too.
    const QStringList ids = idsOf(reqs);
    const QVector<QVector<int>> a =
        DependencyGraphLayout::orderLayers(reqs.size(), first.edges(),
                                           layersOf(first, reqs.size()), ids);
    const QVector<QVector<int>> b =
        DependencyGraphLayout::orderLayers(reqs.size(), first.edges(),
                                           layersOf(first, reqs.size()), ids);
    QCOMPARE(a, b);
}

void TestGraphLayout::alignedAndCentered()
{
    // Mixed-height layers: 003/004 on layer 0 (2 rows), 002 on layer 1,
    // 001 on layer 2. The two single-row layers must be vertically centred
    // around layer 0 and everything must stay non-negative.
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active"),
                                QString(), QStringList()));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active"),
                                QString(),
                                QStringList() << QStringLiteral("REQ-SW-PL-001")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-003"), QStringLiteral("active"),
                                QString(),
                                QStringList() << QStringLiteral("REQ-SW-PL-001")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-004"), QStringLiteral("active"),
                                QString(),
                                QStringList() << QStringLiteral("REQ-SW-PL-002")));

    const DependencyGraphData data = DependencyGraphData::build(reqs);
    QCOMPARE(data.layerFor(2), 0); // PL-003
    QCOMPARE(data.layerFor(3), 0); // PL-004
    QCOMPARE(data.layerFor(1), 1); // PL-002
    QCOMPARE(data.layerFor(0), 2); // PL-001

    // Nodes in one layer share the same x; x increases by layer.
    QCOMPARE(data.positionFor(2).x(), data.positionFor(3).x());
    QVERIFY(data.positionFor(2).x() < data.positionFor(1).x());
    QVERIFY(data.positionFor(1).x() < data.positionFor(0).x());

    // y is monotonic along the row order within a layer and never negative
    // (the centering offset must not push any node above the scene).
    QVERIFY(data.positionFor(2).y() < data.positionFor(3).y());
    for (const GraphNode &node : data.nodes())
        QVERIFY2(node.pos.y() >= 0.0, "centering must never produce negative y");
    QVERIFY(data.positionFor(1).y() > 0.0);
    QVERIFY(data.positionFor(0).y() > 0.0);

    // Size-aware X: a very long title widens the layer gap so nodes never
    // overlap. The widening applies to the gap AFTER the wide layer, so put
    // the long title on PL-003 (layer 0) and check the layer0→layer1 gap.
    QVector<Requirement> wideReqs = reqs;
    wideReqs[2].title =
        QStringLiteral("REQ-SW-PL-003 with an extremely long title text");
    const DependencyGraphData wideData = DependencyGraphData::build(wideReqs);
    const qreal gap = wideData.positionFor(1).x() - wideData.positionFor(2).x();
    QVERIFY2(gap > 260.0, "wide layer must push the next layer further apart");
}

void TestGraphLayout::cycleResidualLayerStable()
{
    // Dependency cycle fixture: PL-040 and PL-041 depend on each other.
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active"),
                                QString(), QStringList()));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-040"), QStringLiteral("active"),
                                QString(),
                                QStringList() << QStringLiteral("REQ-SW-PL-041")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-041"), QStringLiteral("active"),
                                QString(),
                                QStringList() << QStringLiteral("REQ-SW-PL-040")));

    const DependencyGraphData first = DependencyGraphData::build(reqs);
    const DependencyGraphData second = DependencyGraphData::build(reqs);
    QVERIFY(first.hasCycle());
    QVERIFY(second.hasCycle());
    QCOMPARE(first.nodes().size(), 3);

    // Residual layer must be deterministic and both builds must terminate with
    // identical layers and positions.
    for (const GraphNode &node : first.nodes()) {
        const int idx = node.reqIndex;
        QCOMPARE(first.layerFor(idx), second.layerFor(idx));
        QCOMPARE(first.positionFor(idx), second.positionFor(idx));
    }
    QCOMPARE(first.layerFor(0), 0); // PL-001 acyclic
    QCOMPARE(first.layerFor(1), 1); // cycle pair -> residual layer
    QCOMPARE(first.layerFor(2), 1);
}

void TestGraphLayout::parentEdgesDoNotBreakLayering()
{
    // Parent edges connect arbitrary levels — including "backward" relative to
    // the dependency direction (PL-004 parent PL-001 goes layer 0 → layer 2) —
    // but the dependency layering invariant must hold regardless.
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active"),
                                QString(), QStringList()));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active"),
                                QStringLiteral("REQ-SW-PL-001"),
                                QStringList() << QStringLiteral("REQ-SW-PL-001")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-003"), QStringLiteral("active"),
                                QStringLiteral("REQ-SW-PL-002"),
                                QStringList() << QStringLiteral("REQ-SW-PL-002")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-004"), QStringLiteral("active"),
                                QStringLiteral("REQ-SW-PL-001"),
                                QStringList() << QStringLiteral("REQ-SW-PL-003")));

    const DependencyGraphData data = DependencyGraphData::build(reqs);
    QVERIFY(!data.hasCycle());

    int parentEdges = 0;
    for (const GraphEdge &edge : data.edges()) {
        if (edge.kind == GraphEdge::Parent) {
            ++parentEdges;
            continue;
        }
        QVERIFY2(edge.from >= 0 && edge.to >= 0, "resolved dependency edge");
        QVERIFY2(data.layerFor(edge.from) < data.layerFor(edge.to),
                 qPrintable(QStringLiteral("dependency %1 -> %2 must go upward in layers")
                                .arg(data.nodes().at(edge.from).id,
                                     data.nodes().at(edge.to).id)));
    }
    // PL-002..PL-004 all carry parent links (one of them backward) — they must
    // not have influenced the dependency layering.
    QCOMPARE(parentEdges, 3);
}

// No QTEST_GUILESS_MAIN here: all test classes share one binary whose main
// lives in test_main.cpp.
