#include <QtTest>
#include <QBuffer>
#include <QVector>

#include "TraceabilityMatrixModel.h"
#include "MatrixExporter.h"
#include "RequirementsParser.h"

using namespace Daqster;

class TestMatrix : public QObject
{
    Q_OBJECT

private slots:
    void testRowCount()
    {
        TraceabilityMatrixModel model;
        QVector<Requirement> reqs;
        model.setRequirements(reqs);
        QCOMPARE(model.rowCount(), 0);

        Requirement req;
        req.id = "REQ-SW-001";
        req.title = "Test";
        req.status = "ACTIVE";
        reqs.append(req);
        model.setRequirements(reqs);
        QCOMPARE(model.rowCount(), 1);
    }

    void testColumnCount()
    {
        TraceabilityMatrixModel model;
        QCOMPARE(model.columnCount(), TraceabilityMatrixModel::ColumnCount);
    }

    void testData()
    {
        TraceabilityMatrixModel model;
        QVector<Requirement> reqs;
        Requirement req;
        req.id = "REQ-SW-001";
        req.title = "Test Title";
        req.status = "ACTIVE";
        req.priority = "High";
        req.parentId = "REQ-SW-000";
        req.dependencies = QStringList() << "REQ-SW-002" << "REQ-SW-003";
        req.commits = "abc123";
        req.code = "src/test.cpp";
        req.tests = "unit tests";
        req.section = "active";
        reqs.append(req);
        model.setRequirements(reqs);

        QCOMPARE(model.data(model.index(0, TraceabilityMatrixModel::IdColumn)), QVariant("REQ-SW-001"));
        QCOMPARE(model.data(model.index(0, TraceabilityMatrixModel::TitleColumn)), QVariant("Test Title"));
        QCOMPARE(model.data(model.index(0, TraceabilityMatrixModel::StatusColumn)), QVariant("ACTIVE"));
        QCOMPARE(model.data(model.index(0, TraceabilityMatrixModel::PriorityColumn)), QVariant("High"));
        QCOMPARE(model.data(model.index(0, TraceabilityMatrixModel::ParentColumn)), QVariant("REQ-SW-000"));
        QCOMPARE(model.data(model.index(0, TraceabilityMatrixModel::DependenciesColumn)), QVariant("REQ-SW-002, REQ-SW-003"));
        QCOMPARE(model.data(model.index(0, TraceabilityMatrixModel::CommitsColumn)), QVariant("abc123"));
        QCOMPARE(model.data(model.index(0, TraceabilityMatrixModel::CodeColumn)), QVariant("src/test.cpp"));
        QCOMPARE(model.data(model.index(0, TraceabilityMatrixModel::TestsColumn)), QVariant("unit tests"));
        QCOMPARE(model.data(model.index(0, TraceabilityMatrixModel::SectionColumn)), QVariant("active"));
    }

    void testStatusFilter()
    {
        TraceabilityMatrixModel model;
        QVector<Requirement> reqs;
        Requirement req1;
        req1.id = "REQ-SW-001";
        req1.status = "ACTIVE";
        reqs.append(req1);
        Requirement req2;
        req2.id = "REQ-SW-002";
        req2.status = "DONE";
        reqs.append(req2);
        model.setRequirements(reqs);

        QCOMPARE(model.statusFilter(), "All");
        model.setStatusFilter("ACTIVE");
        QCOMPARE(model.statusFilter(), "ACTIVE");
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0, TraceabilityMatrixModel::IdColumn)), QVariant("REQ-SW-001"));

        model.setStatusFilter("DONE");
        QCOMPARE(model.statusFilter(), "DONE");
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0, TraceabilityMatrixModel::IdColumn)), QVariant("REQ-SW-002"));

        model.setStatusFilter("All");
        QCOMPARE(model.statusFilter(), "All");
        QCOMPARE(model.rowCount(), 2);
    }

    void testDomainFilter()
    {
        TraceabilityMatrixModel model;
        QVector<Requirement> reqs;
        Requirement req1;
        req1.id = "REQ-SW-001";
        req1.status = "ACTIVE";
        reqs.append(req1);
        Requirement req2;
        req2.id = "REQ-PLG-001";
        req2.status = "DONE";
        reqs.append(req2);
        model.setRequirements(reqs);

        QCOMPARE(model.domainFilter(), "");
        model.setDomainFilter("SW");
        QCOMPARE(model.domainFilter(), "SW");
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0, TraceabilityMatrixModel::IdColumn)), QVariant("REQ-SW-001"));

        model.setDomainFilter("PLG");
        QCOMPARE(model.domainFilter(), "PLG");
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0, TraceabilityMatrixModel::IdColumn)), QVariant("REQ-PLG-001"));

        model.setDomainFilter("");
        QCOMPARE(model.domainFilter(), "");
        QCOMPARE(model.rowCount(), 2);
    }
};

QTEST_MAIN(TestMatrix)
#include "test_matrix.moc"