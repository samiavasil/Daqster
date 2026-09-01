#include <QtTest>

#include "RequirementsModel.h"
#include "RequirementsParser.h"
#include "test_model.h"

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

// Fixture shape (mirrors the real data: REQ-SW-PL-001 has 10 children):
//   REQ-SW-PL-001            root (parentless)
//   REQ-SW-PL-002 .. 011     10 children of REQ-SW-PL-001
//   REQ-SW-PL-014            child of REQ-SW-PL-002 (3rd level)
//   REQ-SW-PL-031            child of REQ-SW-PL-014 (4th level)
//   REQ-SW-PL-040            second root
//   REQ-SW-PL-050            dangling parentId (REQ-SW-PL-999 does not exist)
//   REQ-SW-PL-060 <-> REQ-SW-PL-061    parent cycle (broken gracefully)
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
                                QStringLiteral("REQ-SW-PL-002")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-031"), QStringLiteral("active"),
                                QStringLiteral("REQ-SW-PL-014")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-040"), QStringLiteral("active"),
                                QString()));
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

} // namespace

void TestModel::contract_sectionsMode()
{
    RequirementsModel model;
    model.setRequirements(makeFixture());
    model.setViewMode(RequirementsModel::ViewMode::Sections);

    QCOMPARE(model.rowCount(QModelIndex()), 2); // "active" + "archive"

    QVector<QModelIndex> parents;
    parents.append(QModelIndex());
    const int rootRows = model.rowCount(QModelIndex());
    for (int s = 0; s < rootRows; ++s)
        parents.append(model.index(s, 0, QModelIndex()));

    // Every requirement node is also a candidate parent (must have 0 rows in
    // Sections mode, so it contributes nothing to the round-trip).
    const QVector<Requirement> fixture = makeFixture();
    for (const Requirement &req : fixture) {
        const QModelIndex idx = model.indexForId(req.id);
        if (idx.isValid())
            parents.append(idx);
    }

    for (const QModelIndex &p : parents) {
        const int rows = model.rowCount(p);
        for (int r = 0; r < rows; ++r) {
            const QModelIndex child = model.index(r, 0, p);
            QVERIFY2(child.isValid(),
                     qPrintable(QStringLiteral("child %1 under parent must be valid").arg(r)));
            QVERIFY2(model.parent(child) == p,
                     qPrintable(QStringLiteral("parent(index(r,0,p)) must round-trip p")));
        }
        // rowCount must be exact: one row past the end is invalid.
        QVERIFY2(!model.index(rows, 0, p).isValid(),
                 qPrintable(QStringLiteral("index(rowCount(p),0,p) must be invalid")));
    }
}

void TestModel::contract_hierarchyMode()
{
    RequirementsModel model;
    model.setRequirements(makeFixture());
    model.setViewMode(RequirementsModel::ViewMode::Hierarchy);

    QCOMPARE(model.rowCount(QModelIndex()), 2); // "active" + "archive"

    QVector<QModelIndex> parents;
    parents.append(QModelIndex());
    const int rootRows = model.rowCount(QModelIndex());
    for (int s = 0; s < rootRows; ++s)
        parents.append(model.index(s, 0, QModelIndex()));

    // Every requirement node is a candidate parent in Hierarchy mode.
    const QVector<Requirement> fixture = makeFixture();
    for (const Requirement &req : fixture) {
        const QModelIndex idx = model.indexForId(req.id);
        QVERIFY2(idx.isValid(),
                 qPrintable(QStringLiteral("indexForId(%1) must be valid").arg(req.id)));
        parents.append(idx);
    }

    for (const QModelIndex &p : parents) {
        const int rows = model.rowCount(p);
        for (int r = 0; r < rows; ++r) {
            const QModelIndex child = model.index(r, 0, p);
            QVERIFY2(child.isValid(),
                     qPrintable(QStringLiteral("child %1 under parent must be valid").arg(r)));
            QVERIFY2(model.parent(child) == p,
                     qPrintable(QStringLiteral("parent(index(r,0,p)) must round-trip p")));
        }
        QVERIFY2(!model.index(rows, 0, p).isValid(),
                 qPrintable(QStringLiteral("index(rowCount(p),0,p) must be invalid")));
    }
}

void TestModel::hierarchy_shape()
{
    RequirementsModel model;
    model.setRequirements(makeFixture());
    model.setViewMode(RequirementsModel::ViewMode::Hierarchy);

    const QModelIndex activeSection = model.index(0, 0, QModelIndex());
    const QModelIndex archiveSection = model.index(1, 0, QModelIndex());

    // Root rows are the sections; sections are sorted alphabetically.
    QCOMPARE(model.rowCount(QModelIndex()), 2);
    QCOMPARE(model.data(activeSection).toString(), QStringLiteral("active"));
    QCOMPARE(model.data(archiveSection).toString(), QStringLiteral("archive"));

    // Sections mode is flat: the active section holds every active requirement.
    model.setViewMode(RequirementsModel::ViewMode::Sections);
    QCOMPARE(model.rowCount(activeSection), 17);
    QCOMPARE(model.rowCount(archiveSection), 1);

    // Hierarchy mode nests by parentId: top-level nodes per section.
    model.setViewMode(RequirementsModel::ViewMode::Hierarchy);
    QCOMPARE(model.rowCount(activeSection), 5); // PL-001, PL-040, PL-050, PL-060, PL-061
    QCOMPARE(model.rowCount(archiveSection), 1); // PL-080

    // REQ-SW-PL-001 has its 10 children.
    const QModelIndex root = model.indexForId(QStringLiteral("REQ-SW-PL-001"));
    QVERIFY(root.isValid());
    QCOMPARE(model.rowCount(root), 10);

    // Nested levels.
    const QModelIndex sw002 = model.indexForId(QStringLiteral("REQ-SW-PL-002"));
    QVERIFY(sw002.isValid());
    QCOMPARE(model.rowCount(sw002), 1); // REQ-SW-PL-014

    const QModelIndex sw021 = model.indexForId(QStringLiteral("REQ-SW-PL-014"));
    QVERIFY(sw021.isValid());
    QCOMPARE(model.rowCount(sw021), 1); // REQ-SW-PL-031

    const QModelIndex sw031 = model.indexForId(QStringLiteral("REQ-SW-PL-031"));
    QVERIFY(sw031.isValid());
    QCOMPARE(model.rowCount(sw031), 0);

    const QModelIndex sw040 = model.indexForId(QStringLiteral("REQ-SW-PL-040"));
    QVERIFY(sw040.isValid());
    QCOMPARE(model.rowCount(sw040), 0);
}

void TestModel::hierarchy_nesting()
{
    RequirementsModel model;
    model.setRequirements(makeFixture());
    model.setViewMode(RequirementsModel::ViewMode::Hierarchy);

    const QModelIndex activeSection = model.index(0, 0, QModelIndex());

    // Direct parent chain: PL-031 -> PL-014 -> PL-002 -> active section.
    const QModelIndex sw031 = model.indexForId(QStringLiteral("REQ-SW-PL-031"));
    const QModelIndex sw021 = model.indexForId(QStringLiteral("REQ-SW-PL-014"));
    const QModelIndex sw002 = model.indexForId(QStringLiteral("REQ-SW-PL-002"));
    const QModelIndex sw001 = model.indexForId(QStringLiteral("REQ-SW-PL-001"));
    QVERIFY(sw031.isValid());
    QVERIFY(sw021.isValid());
    QVERIFY(sw002.isValid());
    QVERIFY(sw001.isValid());

    QVERIFY2(model.parent(sw031) == sw021, "PL-031 parent must be PL-014");
    QVERIFY2(model.parent(sw021) == sw002, "PL-014 parent must be PL-002");
    QVERIFY2(model.parent(sw002) == sw001, "PL-002 parent must be PL-001");
    QVERIFY2(model.parent(sw001) == activeSection,
             "PL-001 (root) parent must be the active section");

    // The 10 children of PL-001 are exactly PL-002 .. PL-011.
    QSet<QString> childIds;
    for (int r = 0; r < model.rowCount(sw001); ++r) {
        const QModelIndex child = model.index(r, 0, sw001);
        QVERIFY(child.isValid());
        const Requirement *req = model.requirementAt(child);
        QVERIFY2(req, "child must map back to a Requirement");
        childIds.insert(req->id);
    }
    QCOMPARE(childIds.size(), 10);
    for (int i = 2; i <= 11; ++i) {
        const QString id =
            QStringLiteral("REQ-SW-PL-%1").arg(i, 3, 10, QLatin1Char('0'));
        QVERIFY2(childIds.contains(id),
                 qPrintable(QStringLiteral("REQ-SW-PL-001 must have child %1").arg(id)));
    }
}

void TestModel::indexForId_behaviour()
{
    RequirementsModel model;
    model.setRequirements(makeFixture());

    const QVector<Requirement> fixture = makeFixture();
    for (const RequirementsModel::ViewMode mode :
         {RequirementsModel::ViewMode::Sections,
          RequirementsModel::ViewMode::Hierarchy}) {
        model.setViewMode(mode);

        // Every requirement is reachable by ID in both modes.
        for (const Requirement &req : fixture) {
            const QModelIndex idx = model.indexForId(req.id);
            QVERIFY2(idx.isValid(),
                     qPrintable(QStringLiteral("%1: indexForId(%2) must be valid")
                                    .arg(mode == RequirementsModel::ViewMode::Sections
                                             ? QStringLiteral("Sections")
                                             : QStringLiteral("Hierarchy"),
                                         req.id)));
            const Requirement *at = model.requirementAt(idx);
            QVERIFY(at);
            QCOMPARE(at->id, req.id);
        }

        // Unknown ID -> invalid index.
        QVERIFY(!model.indexForId(QStringLiteral("REQ-UNKNOWN-999")).isValid());

        // Case-insensitive lookup.
        const QModelIndex lower = model.indexForId(QStringLiteral("req-sw-pl-040"));
        QVERIFY(lower.isValid());
        QCOMPARE(model.requirementAt(lower)->id, QStringLiteral("REQ-SW-PL-040"));
    }
}

void TestModel::danglingParent_isTopLevel()
{
    RequirementsModel model;
    model.setRequirements(makeFixture());
    model.setViewMode(RequirementsModel::ViewMode::Hierarchy);

    const QModelIndex activeSection = model.index(0, 0, QModelIndex());
    const QModelIndex sw050 = model.indexForId(QStringLiteral("REQ-SW-PL-050"));
    QVERIFY(sw050.isValid());

    // Dangling parentId must not crash the model: the requirement stays a
    // top-level node under its section and has no children.
    QVERIFY2(model.parent(sw050) == activeSection,
             "dangling-parent requirement must be top-level");
    QCOMPARE(model.rowCount(sw050), 0);
}

void TestModel::cycle_safe()
{
    RequirementsModel model;
    model.setRequirements(makeFixture());
    model.setViewMode(RequirementsModel::ViewMode::Hierarchy);

    const QModelIndex activeSection = model.index(0, 0, QModelIndex());

    // Parent cycle PL-060 <-> PL-061 must not hang or crash the model. The
    // cycle is broken: both requirements render as top-level nodes with no
    // children.
    const QModelIndex sw060 = model.indexForId(QStringLiteral("REQ-SW-PL-060"));
    const QModelIndex sw061 = model.indexForId(QStringLiteral("REQ-SW-PL-061"));
    QVERIFY(sw060.isValid());
    QVERIFY(sw061.isValid());

    QVERIFY2(model.parent(sw060) == activeSection, "cycle node PL-060 must be top-level");
    QVERIFY2(model.parent(sw061) == activeSection, "cycle node PL-061 must be top-level");
    QCOMPARE(model.rowCount(sw060), 0);
    QCOMPARE(model.rowCount(sw061), 0);

    // Rendering the whole tree must terminate (rowCount walk over every node).
    const int activeRows = model.rowCount(activeSection);
    QVERIFY(activeRows > 0);
    for (int r = 0; r < activeRows; ++r) {
        const QModelIndex top = model.index(r, 0, activeSection);
        QVERIFY(top.isValid());
        // Walking descendants via parent() round-trip must stay consistent.
        QVERIFY2(model.parent(top) == activeSection, "top-level node parent round-trip");
    }
}

void TestModel::requirementRole_data()
{
    RequirementsModel model;
    model.setRequirements(makeFixture());

    const QModelIndex idx = model.indexForId(QStringLiteral("REQ-SW-PL-040"));
    QVERIFY(idx.isValid());

    const QVariant value = model.data(idx, RequirementsModel::RequirementRole);
    QVERIFY(value.isValid());
    const Requirement req = value.value<Requirement>();
    QCOMPARE(req.id, QStringLiteral("REQ-SW-PL-040"));
    QCOMPARE(req.section, QStringLiteral("active"));
}

// No QTEST_GUILESS_MAIN here: the three test classes share one binary whose
// main lives in test_main.cpp.
