#include <QtTest>
#include <QBuffer>
#include <QVector>
#include <QFile>
#include <QTextStream>

#include "MatrixExporter.h"
#include "RequirementsParser.h"

using namespace Daqster;

class TestExporter : public QObject
{
    Q_OBJECT

private slots:
    void testExportMarkdown()
    {
        QVector<Requirement> reqs;
        Requirement req;
        req.id = "REQ-SW-001";
        req.title = "Test Requirement";
        req.status = "ACTIVE";
        req.priority = "High";
        req.parentId = "REQ-SW-000";
        req.dependencies = QStringList() << "REQ-SW-002";
        req.commits = "abc123";
        req.code = "src/test.cpp";
        req.tests = "unit tests";
        req.section = "active";
        reqs.append(req);

        QBuffer buffer;
        buffer.open(QIODevice::ReadWrite);
        QVERIFY(MatrixExporter::exportMarkdown(reqs, buffer));
        buffer.seek(0);
        QString content = buffer.readAll();

        QVERIFY(content.contains("# Traceability Matrix — Requirements Manager & Framework Tools"));
        QVERIFY(content.contains("REQ-SW-001"));
        QVERIFY(content.contains("Test Requirement"));
        QVERIFY(content.contains("ACTIVE"));
        QVERIFY(content.contains("REQ-SW-000"));
        QVERIFY(content.contains("REQ-SW-002"));
        QVERIFY(content.contains("abc123"));
        QVERIFY(content.contains("src/test.cpp"));
        QVERIFY(content.contains("unit tests"));
    }

    void testExportCsv()
    {
        QVector<Requirement> reqs;
        Requirement req;
        req.id = "REQ-SW-001";
        req.title = "Test, Requirement";
        req.status = "ACTIVE";
        req.priority = "High";
        req.parentId = "REQ-SW-000";
        req.dependencies = QStringList() << "REQ-SW-002";
        req.commits = "abc\"123";
        req.code = "src/test.cpp";
        req.tests = "unit\nnewline";
        req.section = "active";
        reqs.append(req);

        QBuffer buffer;
        buffer.open(QIODevice::ReadWrite);
        QVERIFY(MatrixExporter::exportCsv(reqs, buffer));
        buffer.seek(0);
        QString content = buffer.readAll();

        QVERIFY(content.contains("ID,Title,Status,Priority,Parent,Dependencies,Commits,Code,Tests,Section"));
        QVERIFY(content.contains("REQ-SW-001"));
        QVERIFY(content.contains("Test, Requirement"));
        QVERIFY(content.contains("REQ-SW-000"));
        QVERIFY(content.contains("REQ-SW-002"));
        QVERIFY(content.contains("abc\"\"123"));
        QVERIFY(content.contains("src/test.cpp"));
        QVERIFY(content.contains(QStringLiteral("unit\nnewline")));
    }

    void testExportJson()
    {
        QVector<Requirement> reqs;
        Requirement req;
        req.id = "REQ-SW-001";
        req.title = "Test Requirement";
        req.status = "ACTIVE";
        req.priority = "High";
        req.assignee = "Implementation";
        req.date = "2026-07-31";
        req.parentId = "REQ-SW-000";
        req.dependencies = QStringList() << "REQ-SW-002";
        req.description = "Test description";
        req.traceability = "Test traceability";
        req.commits = "abc123";
        req.code = "src/test.cpp";
        req.tests = "unit tests";
        req.fileName = "REQ-SW-001-test.md";
        req.section = "active";
        req.acceptanceCriteria = QStringList() << "Criterion 1" << "Criterion 2";
        req.criteriaDone = QVector<bool>() << true << false;
        reqs.append(req);

        QBuffer buffer;
        buffer.open(QIODevice::ReadWrite);
        QVERIFY(MatrixExporter::exportJson(reqs, buffer));
        buffer.seek(0);
        QByteArray jsonData = buffer.readAll();

        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        QVERIFY(doc.isArray());
        QJsonArray array = doc.array();
        QCOMPARE(array.size(), 1);

        QJsonObject obj = array[0].toObject();
        QCOMPARE(obj["id"].toString(), "REQ-SW-001");
        QCOMPARE(obj["title"].toString(), "Test Requirement");
        QCOMPARE(obj["status"].toString(), "ACTIVE");
        QCOMPARE(obj["priority"].toString(), "High");
        QCOMPARE(obj["assignee"].toString(), "Implementation");
        QCOMPARE(obj["date"].toString(), "2026-07-31");
        QCOMPARE(obj["parentId"].toString(), "REQ-SW-000");
        QCOMPARE(obj["dependencies"].toArray().size(), 1);
        QCOMPARE(obj["dependencies"].toArray()[0].toString(), "REQ-SW-002");
        QCOMPARE(obj["description"].toString(), "Test description");
        QCOMPARE(obj["traceability"].toString(), "Test traceability");
        QCOMPARE(obj["commits"].toString(), "abc123");
        QCOMPARE(obj["code"].toString(), "src/test.cpp");
        QCOMPARE(obj["tests"].toString(), "unit tests");
        QCOMPARE(obj["fileName"].toString(), "REQ-SW-001-test.md");
        QCOMPARE(obj["section"].toString(), "active");
        QCOMPARE(obj["acceptanceCriteria"].toArray().size(), 2);
        QCOMPARE(obj["acceptanceCriteria"].toArray()[0].toString(), "Criterion 1");
        QCOMPARE(obj["acceptanceCriteria"].toArray()[1].toString(), "Criterion 2");
        QCOMPARE(obj["criteriaDone"].toArray()[0].toBool(), true);
        QCOMPARE(obj["criteriaDone"].toArray()[1].toBool(), false);
    }

    void testBuildSummary()
    {
        QVector<Requirement> reqs;
        Requirement req1;
        req1.id = "REQ-SW-001";
        req1.status = "ACTIVE";
        req1.acceptanceCriteria = QStringList() << "Criterion 1" << "Criterion 2";
        req1.criteriaDone = QVector<bool>() << true << false;
        req1.dependencies = QStringList() << "REQ-SW-002";
        reqs.append(req1);

        Requirement req2;
        req2.id = "REQ-SW-002";
        req2.status = "DONE";
        req2.acceptanceCriteria = QStringList() << "Criterion 1";
        req2.criteriaDone = QVector<bool>() << true;
        reqs.append(req2);

        Requirement req3;
        req3.id = "REQ-SW-003";
        req3.status = "CANCELLED";
        req3.acceptanceCriteria.clear();
        req3.criteriaDone.clear();
        reqs.append(req3);

        QString summary = MatrixExporter::buildSummary(reqs);

        QVERIFY(summary.contains("Total requirements: 3"));
        QVERIFY(summary.contains("Status: ACTIVE 1 | CANCELLED 1 | DONE 1"));
        QVERIFY(summary.contains("Completion: 2/3 acceptance criteria (66.7%)"));
        QVERIFY(summary.contains("With dependencies: 1"));
        QVERIFY(summary.contains("Dangling references: 0"));
        QVERIFY(summary.contains("Cycles: 0"));
    }

    void testExportEmpty()
    {
        QVector<Requirement> reqs;

        QBuffer buffer;
        buffer.open(QIODevice::ReadWrite);
        QVERIFY(MatrixExporter::exportMarkdown(reqs, buffer));
        buffer.seek(0);
        QString content = buffer.readAll();
        QVERIFY(content.contains("# Traceability Matrix"));

        QBuffer buffer2;
        buffer2.open(QIODevice::ReadWrite);
        QVERIFY(MatrixExporter::exportCsv(reqs, buffer2));
        buffer2.seek(0);
        content = buffer2.readAll();
        QVERIFY(content.contains("ID,Title,Status,Priority,Parent,Dependencies,Commits,Code,Tests,Section"));

        QBuffer buffer3;
        buffer3.open(QIODevice::ReadWrite);
        QVERIFY(MatrixExporter::exportJson(reqs, buffer3));
        buffer3.seek(0);
        QByteArray jsonData = buffer3.readAll();
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        QVERIFY(doc.isArray());
        QCOMPARE(doc.array().size(), 0);
    }
};

QTEST_MAIN(TestExporter)
#include "test_exporter.moc"