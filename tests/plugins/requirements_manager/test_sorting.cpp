#include <QtTest>

#include "RequirementsModel.h"
#include "RequirementsSortProxyModel.h"
#include "TraceabilityMatrixModel.h"
#include "test_sorting.h"

using namespace Daqster;

namespace {

Requirement makeRequirement(const QString &id, const QString &status,
                            const QString &section = QStringLiteral("active"))
{
    Requirement req;
    req.id = id;
    req.title = QStringLiteral("Title of %1").arg(id);
    req.status = status;
    req.priority = QStringLiteral("Medium");
    req.section = section;
    req.repo = QStringLiteral("public");
    return req;
}

// Deliberately unsorted: REQ-SW-PL-002 before REQ-SW-PL-001, DONE before ACTIVE,
// and one archive requirement so both sections exist.
QVector<Requirement> unsortedFixture()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("ACTIVE")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("DONE")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-003"), QStringLiteral("ACTIVE")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-004"), QStringLiteral("DONE"),
                                QStringLiteral("archive")));
    return reqs;
}

// Visible IDs of a sorted proxy, walking the tree in view order.
QStringList visibleIds(const QAbstractItemModel &model)
{
    QStringList ids;
    for (int section = 0; section < model.rowCount(); ++section) {
        const QModelIndex sectionIndex = model.index(section, 0);
        for (int row = 0; row < model.rowCount(sectionIndex); ++row) {
            ids.append(model.index(row, 0, sectionIndex).data(Qt::DisplayRole).toString());
        }
    }
    return ids;
}

} // namespace

void TestSorting::treeSortsWithinSection_data()
{
    QTest::addColumn<int>("column");
    QTest::addColumn<int>("order");
    QTest::addColumn<QStringList>("expectedActive");

    QTest::newRow("id ascending") << int(RequirementsModel::IdColumn) << int(Qt::AscendingOrder)
                                  << QStringList{QStringLiteral("REQ-SW-PL-001"),
                                                 QStringLiteral("REQ-SW-PL-002"),
                                                 QStringLiteral("REQ-SW-PL-003")};
    QTest::newRow("id descending") << int(RequirementsModel::IdColumn) << int(Qt::DescendingOrder)
                                   << QStringList{QStringLiteral("REQ-SW-PL-003"),
                                                  QStringLiteral("REQ-SW-PL-002"),
                                                  QStringLiteral("REQ-SW-PL-001")};
    QTest::newRow("status ascending") << int(RequirementsModel::StatusColumn)
                                      << int(Qt::AscendingOrder)
                                      << QStringList{QStringLiteral("REQ-SW-PL-002"),
                                                     QStringLiteral("REQ-SW-PL-003"),
                                                     QStringLiteral("REQ-SW-PL-001")};
}

void TestSorting::treeSortsWithinSection()
{
    QFETCH(int, column);
    QFETCH(int, order);
    QFETCH(QStringList, expectedActive);

    RequirementsModel model;
    model.setRequirements(unsortedFixture());

    RequirementsSortProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.setDynamicSortFilter(true);
    proxy.sort(column, static_cast<Qt::SortOrder>(order));

    const QStringList ids = visibleIds(proxy);
    QVERIFY2(ids.size() == 4, qPrintable(ids.join(QLatin1Char(','))));

    // First section is "active": exactly the three fixtures from it, reordered.
    QCOMPARE(ids.mid(0, expectedActive.size()), expectedActive);
    // The requirement from archive/ stays in the second section.
    QCOMPARE(ids.last(), QStringLiteral("REQ-SW-PL-004"));
}

void TestSorting::treeSectionsKeepSourceOrder()
{
    RequirementsModel model;
    model.setRequirements(unsortedFixture());

    RequirementsSortProxyModel proxy;
    proxy.setSourceModel(&model);

    // "active" must stay first no matter which order the user picks — sorting
    // the section rows would move a whole subtree.
    proxy.sort(RequirementsModel::IdColumn, Qt::DescendingOrder);
    QCOMPARE(proxy.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("active"));
    QCOMPARE(proxy.index(1, 0).data(Qt::DisplayRole).toString(), QStringLiteral("archive"));

    proxy.sort(RequirementsModel::StatusColumn, Qt::AscendingOrder);
    QCOMPARE(proxy.index(0, 0).data(Qt::DisplayRole).toString(), QStringLiteral("active"));
}

void TestSorting::matrixSortsEveryColumn_data()
{
    QTest::addColumn<int>("column");
    QTest::addColumn<int>("order");
    QTest::addColumn<QString>("expectedFirstId");

    QTest::newRow("id ascending") << int(TraceabilityMatrixModel::IdColumn)
                                  << int(Qt::AscendingOrder)
                                  << QStringLiteral("REQ-SW-PL-001");
    QTest::newRow("id descending") << int(TraceabilityMatrixModel::IdColumn)
                                   << int(Qt::DescendingOrder)
                                   << QStringLiteral("REQ-SW-PL-004");
    QTest::newRow("status ascending") << int(TraceabilityMatrixModel::StatusColumn)
                                      << int(Qt::AscendingOrder)
                                      << QStringLiteral("REQ-SW-PL-002");
    QTest::newRow("title ascending") << int(TraceabilityMatrixModel::TitleColumn)
                                     << int(Qt::AscendingOrder)
                                     << QStringLiteral("REQ-SW-PL-001");
    QTest::newRow("section descending") << int(TraceabilityMatrixModel::SectionColumn)
                                        << int(Qt::DescendingOrder)
                                        << QStringLiteral("REQ-SW-PL-004");
}

void TestSorting::matrixSortsEveryColumn()
{
    QFETCH(int, column);
    QFETCH(int, order);
    QFETCH(QString, expectedFirstId);

    QVector<Requirement> reqs = unsortedFixture();
    // Distinct titles so the title ordering is deterministic too.
    reqs[0].title = QStringLiteral("Beta");
    reqs[1].title = QStringLiteral("Alpha");
    reqs[2].title = QStringLiteral("Gamma");
    reqs[3].title = QStringLiteral("Delta");

    TraceabilityMatrixModel model;
    model.setRequirements(reqs);

    QSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    proxy.setSortCaseSensitivity(Qt::CaseInsensitive);
    proxy.sort(column, static_cast<Qt::SortOrder>(order));

    QCOMPARE(proxy.rowCount(), 4);
    QCOMPARE(proxy.index(0, TraceabilityMatrixModel::IdColumn).data(Qt::DisplayRole).toString(),
             expectedFirstId);
}

void TestSorting::matrixDefaultOrderKeepsSourceOrder()
{
    TraceabilityMatrixModel model;
    model.setRequirements(unsortedFixture());

    // Without a sort call the model's own order is what the view sees; the
    // filters must not be disturbed by adding the proxy.
    QCOMPARE(model.index(0, TraceabilityMatrixModel::IdColumn).data(Qt::DisplayRole).toString(),
             QStringLiteral("REQ-SW-PL-002"));

    QSortFilterProxyModel proxy;
    proxy.setSourceModel(&model);
    QCOMPARE(proxy.rowCount(), 4);
    QCOMPARE(proxy.index(0, TraceabilityMatrixModel::IdColumn).data(Qt::DisplayRole).toString(),
             QStringLiteral("REQ-SW-PL-002"));

    model.setStatusFilter(QStringLiteral("DONE"));
    QCOMPARE(proxy.rowCount(), 2);
}
