#include <QtTest>

#include "DependencyGraphData.h"
#include "RequirementsParser.h"
#include "test_graph.h"

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

// Acyclic-dependency fixture (mirrors test_model.cpp's shape plus deps):
//   REQ-SW-PL-001            root (parentless)
//   REQ-SW-PL-002 .. 011     10 children of REQ-SW-PL-001
//   REQ-SW-PL-014            child of REQ-SW-PL-002, depends on PL-002 + PL-001
//   REQ-SW-PL-031            child of REQ-SW-PL-014, depends on PL-014
//   REQ-SW-PL-040            second root, depends on PL-001
//   REQ-SW-PL-050            dangling parentId (REQ-SW-PL-999 does not exist)
//   REQ-SW-PL-060 <-> REQ-SW-PL-061    parent cycle (dependency graph stays acyclic)
//   REQ-SW-PL-080            root in archive section
QVector<Requirement> makeFixture()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active"),
                                QString()));
    for (int i = 2; i <= 11; ++i) {
        reqs.append(makeRequirement(
            QStringLiteral("REQ-SW-PL-%1").arg(i, 3, 10, QLatin1Char('0')),
            QStringLiteral("active"), QStringLiteral("REQ-SW-PL-001")));
    }
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-014"), QStringLiteral("active"),
                                QStringLiteral("REQ-SW-PL-002"),
                                QStringList() << QStringLiteral("REQ-SW-PL-002")
                                              << QStringLiteral("REQ-SW-PL-001")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-031"), QStringLiteral("active"),
                                QStringLiteral("REQ-SW-PL-014"),
                                QStringList() << QStringLiteral("REQ-SW-PL-014")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-040"), QStringLiteral("active"),
                                QString(),
                                QStringList() << QStringLiteral("REQ-SW-PL-001")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-050"), QStringLiteral("active"),
                                QStringLiteral("REQ-SW-PL-999")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-060"), QStringLiteral("active"),
                                QStringLiteral("REQ-SW-PL-061")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-061"), QStringLiteral("active"),
                                QStringLiteral("REQ-SW-PL-060")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-080"), QStringLiteral("archive"),
                                QString()));
    return reqs;
}

// Dependency-cycle fixture: REQ-SW-PL-040 and REQ-SW-PL-041 depend on each other.
QVector<Requirement> makeCycleFixture()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active"),
                                QString()));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-040"), QStringLiteral("active"),
                                QString(),
                                QStringList() << QStringLiteral("REQ-SW-PL-041")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-041"), QStringLiteral("active"),
                                QString(),
                                QStringList() << QStringLiteral("REQ-SW-PL-040")));
    return reqs;
}

} // namespace

void TestGraph::nodeAndEdgeCounts()
{
    const QVector<Requirement> fixture = makeFixture();
    const DependencyGraphData data = DependencyGraphData::build(fixture);

    // One node per requirement.
    QCOMPARE(data.nodes().size(), fixture.size());
    QVERIFY(!data.hasCycle()); // only a parent cycle exists, not a dep cycle

    // Parent edges: PL-002..011 (10) -> PL-001, PL-014 -> PL-002,
    // PL-031 -> PL-014, PL-060 <-> PL-061 (2) = 14. PL-040/PL-080 have no
    // parent; PL-050's parent is dangling and must not become an edge.
    int parentEdges = 0;
    int dependencyEdges = 0;
    for (const GraphEdge &edge : data.edges()) {
        if (edge.kind == GraphEdge::Parent)
            ++parentEdges;
        else
            ++dependencyEdges;
    }
    QCOMPARE(parentEdges, 14);

    // Dependency edges: PL-014->PL-002, PL-014->PL-001, PL-031->PL-014,
    // PL-040->PL-001 = 4.
    QCOMPARE(dependencyEdges, 4);

    // Only PL-050's parent reference is dangling.
    QCOMPARE(data.danglingCount(), 1);
}

void TestGraph::danglingRecordedNotRendered()
{
    const DependencyGraphData data = DependencyGraphData::build(makeFixture());

    QVERIFY(data.danglingCount() > 0);
    QVERIFY2(data.danglingIds().contains(QStringLiteral("REQ-SW-PL-999")),
             "unresolvable parentId must be recorded as dangling");

    // Dangling references must never become rendered edges.
    for (const GraphEdge &edge : data.edges()) {
        QVERIFY2(edge.danglingId.isEmpty(),
                 "stored edges must never carry a danglingId");
        QVERIFY2(edge.from >= 0 && edge.to >= 0,
                 "stored edges must have resolved endpoints");
        QVERIFY2(data.nodes().at(edge.from).id != QStringLiteral("REQ-SW-PL-050"),
                 "dangling parent must not render an edge from PL-050");
    }
}

void TestGraph::cycleInputTerminates()
{
    // A dependency cycle must not hang build(): the seen-set guard breaks it
    // and both cycle members land in a residual layer below the DAG.
    const DependencyGraphData data = DependencyGraphData::build(makeCycleFixture());

    QVERIFY2(data.hasCycle(), "dependency cycle must be detected");
    QCOMPARE(data.nodes().size(), 3);

    // Every node must have been laid out (termination + valid layout).
    for (const GraphNode &node : data.nodes())
        QVERIFY2(data.layerFor(node.reqIndex) >= 0,
                 "every node must be assigned to a layer");

    // REQ-SW-PL-001 (index 0) is acyclic -> layer 0; the cycle pair (indexes 1,2)
    // is pushed to the residual layer.
    QCOMPARE(data.layerFor(0), 0);
    QCOMPARE(data.layerFor(1), 1);
    QCOMPARE(data.layerFor(2), 1);
}

void TestGraph::layeredLayoutInvariant()
{
    // The main fixture has no dependency cycles, so every non-dangling
    // dependency edge u -> v ("u depends on v") must satisfy
    // layer[u] < layer[v] — dependencies are always laid out to the right.
    const DependencyGraphData data = DependencyGraphData::build(makeFixture());

    for (const GraphEdge &edge : data.edges()) {
        if (edge.kind != GraphEdge::Dependency)
            continue;
        QVERIFY2(edge.from >= 0 && edge.to >= 0, "resolved dependency edge");
        QVERIFY2(data.layerFor(edge.from) < data.layerFor(edge.to),
                 qPrintable(QStringLiteral("dependency %1 -> %2 must go upward in layers")
                                .arg(data.nodes().at(edge.from).id,
                                     data.nodes().at(edge.to).id)));
    }
}

void TestGraph::ideaNodeAndPromotionEdge()
{
    // REQ-SW-PL-098 AC6: an idea is a dashed node joined to its requirement, and
    // the solid nodes stay requirements only — one idea node and one
    // IdeaPromotion edge, sitting in the requirement's layer.
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active"),
                                QString()));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active"),
                                QStringLiteral("REQ-SW-PL-001")));

    Idea idea;
    idea.id = QStringLiteral("IDEA-002");
    idea.title = QStringLiteral("Alpha");
    idea.status = QStringLiteral("промотирана");
    idea.promotedId = QStringLiteral("REQ-SW-PL-002");

    const DependencyGraphData plain = DependencyGraphData::build(reqs, {});
    const DependencyGraphData data = DependencyGraphData::build(reqs, {idea});

    QCOMPARE(plain.nodes().size(), reqs.size()); // no ideas → the old graph
    QCOMPARE(data.nodes().size(), reqs.size() + 1);
    int ideaNodes = 0;
    int requirementNodes = 0;
    int ideaIndex = -1;
    for (int i = 0; i < data.nodes().size(); ++i) {
        const GraphNode &node = data.nodes().at(i);
        if (node.kind == GraphNode::Idea) {
            ++ideaNodes;
            ideaIndex = i;
            QCOMPARE(node.id, QStringLiteral("IDEA-002"));
            QCOMPARE(node.reqIndex, -1); // not a requirement index
        } else {
            ++requirementNodes;
        }
    }
    QCOMPARE(ideaNodes, 1);
    QCOMPARE(requirementNodes, reqs.size());

    int promotionEdges = 0;
    for (const GraphEdge &edge : data.edges()) {
        if (edge.kind != GraphEdge::IdeaPromotion)
            continue;
        ++promotionEdges;
        QCOMPARE(edge.from, ideaIndex);
        QCOMPARE(data.nodes().at(edge.to).id, QStringLiteral("REQ-SW-PL-002"));
    }
    QCOMPARE(promotionEdges, 1);

    // The idea sits in its requirement's layer, so the picture reads
    // "idea -> requirement" left to right instead of two unrelated columns.
    QCOMPARE(data.nodes().at(ideaIndex).pos.x(), data.positionFor(1).x());
}

void TestGraph::unpromotedIdeaAbsentFromGraph()
{
    // An idea without a requirement has nothing to connect to: it is not a
    // graph node, and the idea->requirement lookup does not become a dangling
    // reference (that finding belongs to the validator, not the picture).
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active"),
                                QString()));

    Idea unpromoted;
    unpromoted.id = QStringLiteral("IDEA-004");
    unpromoted.title = QStringLiteral("Mu");
    unpromoted.status = QStringLiteral("нова");

    Idea broken;
    broken.id = QStringLiteral("IDEA-006");
    broken.title = QStringLiteral("Zeta");
    broken.status = QStringLiteral("промотирана");
    broken.promotedId = QStringLiteral("REQ-SW-PL-999"); // no such requirement

    const DependencyGraphData data = DependencyGraphData::build(reqs, {unpromoted, broken});
    QCOMPARE(data.nodes().size(), 1);
    QCOMPARE(data.edges().size(), 0);
    QCOMPARE(data.danglingCount(), 0);
}

// No QTEST_GUILESS_MAIN here: all test classes share one binary whose main
// lives in test_main.cpp.
