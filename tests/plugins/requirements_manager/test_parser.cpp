#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include "RequirementsParser.h"
#include "test_parser.h"

using namespace Daqster;

namespace {

QString writeFixture(const QString &dirPath, const QString &fileName,
                     const QString &content)
{
    QDir().mkpath(dirPath);
    const QString filePath = QDir(dirPath).filePath(fileName);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return QString();
    file.write(content.toUtf8());
    file.close();
    return filePath;
}

// Full template with metadata, criteria and traceability.
QString fullMetadataContent()
{
    return QStringLiteral(
        "# REQ-SW-001: Requirements Viewer/Editor Tool\n"
        "\n"
        "- **Статус:** ACTIVE\n"
        "- **Приоритет:** High\n"
        "- **Отговорник (роля):** Architect + Implementation\n"
        "- **Дата:** 2026-07-31\n"
        "- **Родител:** —\n"
        "- **Зависи от:** —\n"
        "\n"
        "## Описание\n"
        "\n"
        "Общ GUI инструмент, реализиран като отделен application plugin.\n"
        "\n"
        "## Acceptance Criteria\n"
        "\n"
        "- [x] 1. Първи критерий.\n"
        "- [ ] 2. Втори критерий.\n"
        "\n"
        "## Проследимост\n"
        "\n"
        "- **Коммити:** abc123\n"
        "- **Код:** src/plugins/requirements_manager/\n");
}

// Full metadata with a real parent and dependency list.
QString childMetadataContent()
{
    return QStringLiteral(
        "# REQ-SW-002: Data Model Parsing Extensions\n"
        "\n"
        "- **Статус:** DONE\n"
        "- **Приоритет:** Medium\n"
        "- **Отговорник (роля):** Implementation\n"
        "- **Дата:** 2026-07-20\n"
        "- **Родител:** REQ-SW-001\n"
        "- **Зависи от:** REQ-SW-001, REQ-SW-003\n"
        "\n"
        "## Описание\n"
        "\n"
        "Разширение на парсера с допълнителни метаданни.\n"
        "\n"
        "## Acceptance Criteria\n"
        "\n"
        "- [ ] 1. Parser разпознава Родител и Зависи от.\n"
        "\n"
        "## Проследимост\n"
        "\n"
        "- **Коммити:** —\n");
}

// Minimal requirement without any metadata lines.
QString minimalContent()
{
    return QStringLiteral(
        "# REQ-SW-003: Minimal\n"
        "\n"
        "## Описание\n"
        "\n"
        "Минимално изискване.\n");
}

QString requirementContent(const QString &id, const QString &title,
                           const QString &status, const QString &parentId,
                           const QString &deps)
{
    return QStringLiteral("# %1: %2\n"
                          "\n"
                          "- **Статус:** %3\n"
                          "- **Приоритет:** Low\n"
                          "- **Отговорник (роля):** QA\n"
                          "- **Дата:** 2026-07-01\n"
                          "- **Родител:** %4\n"
                          "- **Зависи от:** %5\n"
                          "\n"
                          "## Описание\n"
                          "\n"
                          "Описание на изискването.\n"
                          "\n"
                          "## Acceptance Criteria\n"
                          "\n"
                          "- [ ] 1. Критерий.\n")
        .arg(id, title, status, parentId, deps);
}

const Requirement *findRequirement(const QVector<Requirement> &requirements,
                                   const QString &id)
{
    for (const Requirement &req : requirements) {
        if (req.id == id)
            return &req;
    }
    return nullptr;
}

} // namespace

void TestParser::parseDirectory_fullMetadata()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();

    const QString activeDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    const QString archiveDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/archive"));

    writeFixture(activeDir, QStringLiteral("REQ-SW-001-requirements-viewer-editor-tool.md"),
                 fullMetadataContent());
    writeFixture(archiveDir, QStringLiteral("REQ-SW-002-archived.md"), childMetadataContent());

    const QVector<Requirement> requirements = RequirementsParser::parseDirectory(baseDir);

    QCOMPARE(requirements.size(), 2);

    const Requirement *req1 = findRequirement(requirements, QStringLiteral("REQ-SW-001"));
    QVERIFY2(req1, "REQ-SW-001 should be parsed");
    QCOMPARE(req1->title, QStringLiteral("Requirements Viewer/Editor Tool"));
    QCOMPARE(req1->status, QStringLiteral("ACTIVE"));
    QCOMPARE(req1->priority, QStringLiteral("High"));
    QCOMPARE(req1->assignee, QStringLiteral("Architect + Implementation"));
    QCOMPARE(req1->date, QStringLiteral("2026-07-31"));
    QVERIFY(req1->parentId.isEmpty());
    QVERIFY(req1->dependencies.isEmpty());
    QVERIFY(req1->description.contains(QStringLiteral("application plugin")));
    QVERIFY(req1->traceability.contains(QStringLiteral("abc123")));
    QCOMPARE(req1->acceptanceCriteria.size(), 2);
    QCOMPARE(req1->acceptanceCriteria.at(0), QStringLiteral("1. Първи критерий."));
    QCOMPARE(req1->acceptanceCriteria.at(1), QStringLiteral("2. Втори критерий."));
    QCOMPARE(req1->criteriaDone.size(), 2);
    QVERIFY(req1->criteriaDone.at(0));
    QVERIFY(!req1->criteriaDone.at(1));
    QCOMPARE(req1->section, QStringLiteral("active"));
    QVERIFY(req1->filePath.endsWith(QStringLiteral("REQ-SW-001-requirements-viewer-editor-tool.md")));
    QCOMPARE(req1->fileName, QStringLiteral("REQ-SW-001-requirements-viewer-editor-tool.md"));

    const Requirement *req2 = findRequirement(requirements, QStringLiteral("REQ-SW-002"));
    QVERIFY2(req2, "REQ-SW-002 should be parsed");
    QCOMPARE(req2->parentId, QStringLiteral("REQ-SW-001"));
    QCOMPARE(req2->dependencies.size(), 2);
    QCOMPARE(req2->dependencies.at(0), QStringLiteral("REQ-SW-001"));
    QCOMPARE(req2->dependencies.at(1), QStringLiteral("REQ-SW-003"));
    QCOMPARE(req2->section, QStringLiteral("archive"));
    QCOMPARE(req2->criteriaDone.size(), 1);
    QVERIFY(!req2->criteriaDone.at(0));
}

void TestParser::parseDirectory_minimalAndDashFields()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();
    const QString activeDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));

    writeFixture(activeDir, QStringLiteral("REQ-SW-003-minimal.md"), minimalContent());

    const QVector<Requirement> requirements = RequirementsParser::parseDirectory(baseDir);

    QCOMPARE(requirements.size(), 1);

    const Requirement *req = findRequirement(requirements, QStringLiteral("REQ-SW-003"));
    QVERIFY2(req, "REQ-SW-003 should be parsed");
    QCOMPARE(req->title, QStringLiteral("Minimal"));
    QVERIFY(req->status.isEmpty());
    QVERIFY(req->priority.isEmpty());
    QVERIFY(req->assignee.isEmpty());
    QVERIFY(req->date.isEmpty());
    QVERIFY(req->parentId.isEmpty());
    QVERIFY(req->dependencies.isEmpty());
    QCOMPARE(req->description, QStringLiteral("Минимално изискване."));
    QVERIFY(req->acceptanceCriteria.isEmpty());
    QVERIFY(req->traceability.isEmpty());
    QCOMPARE(req->section, QStringLiteral("active"));
}

void TestParser::parseDirectory_traceabilityFields()
{
    // "- **Коммити:**" / "- **Код:**" / "- **Тестове:**" lines must populate
    // the structured fields AND still land in the raw traceability text
    // (fall-through, preserving the "traceability.contains(commit)" contract).
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();
    const QString activeDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));

    writeFixture(activeDir, QStringLiteral("REQ-SW-010-traceability-fields.md"),
                 QStringLiteral(
                     "# REQ-SW-010: Traceability Fields\n"
                     "\n"
                     "- **Статус:** ACTIVE\n"
                     "- **Приоритет:** Medium\n"
                     "- **Отговорник (роля):** Implementation\n"
                     "- **Дата:** 2026-07-31\n"
                     "- **Родител:** —\n"
                     "- **Зависи от:** —\n"
                     "\n"
                     "## Описание\n"
                     "\n"
                     "Тест на проследимост полетата.\n"
                     "\n"
                     "## Acceptance Criteria\n"
                     "\n"
                     "- [ ] 1. Критерий.\n"
                     "\n"
                     "## Проследимост\n"
                     "\n"
                     "- **Коммити:** abc123, def456\n"
                     "- **Код:** src/plugins/requirements_manager/\n"
                     "- **Тестове:** Qt5/Qt6 builds + unit tests\n"));

    const QVector<Requirement> requirements = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(requirements.size(), 1);

    const Requirement *req = findRequirement(requirements, QStringLiteral("REQ-SW-010"));
    QVERIFY2(req, "REQ-SW-010 should be parsed");
    QCOMPARE(req->commits, QStringLiteral("abc123, def456"));
    QCOMPARE(req->code, QStringLiteral("src/plugins/requirements_manager/"));
    QCOMPARE(req->tests, QStringLiteral("Qt5/Qt6 builds + unit tests"));

    // The raw traceability text must still contain the same content.
    QVERIFY(req->traceability.contains(QStringLiteral("abc123")));
    QVERIFY(req->traceability.contains(QStringLiteral("def456")));
    QVERIFY(req->traceability.contains(QStringLiteral("src/plugins/requirements_manager/")));
    QVERIFY(req->traceability.contains(QStringLiteral("Qt5/Qt6 builds + unit tests")));

    // A requirement without a Тестове line keeps the field empty.
    writeFixture(activeDir, QStringLiteral("REQ-SW-011-no-tests.md"),
                 QStringLiteral(
                     "# REQ-SW-011: No Tests\n"
                     "\n"
                     "## Описание\n"
                     "\n"
                     "Без тестове.\n"
                     "\n"
                     "## Проследимост\n"
                     "\n"
                     "- **Коммити:** deadbeef\n"));
    const QVector<Requirement> two = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(two.size(), 2);
    const Requirement *req11 = findRequirement(two, QStringLiteral("REQ-SW-011"));
    QVERIFY2(req11, "REQ-SW-011 should be parsed");
    QCOMPARE(req11->commits, QStringLiteral("deadbeef"));
    QVERIFY(req11->tests.isEmpty());
    QVERIFY(req11->traceability.contains(QStringLiteral("deadbeef")));
}

void TestParser::parseDirectory_emptyDir()
{
    // Non-existent directory -> empty result, no crash.
    const QVector<Requirement> none = RequirementsParser::parseDirectory(
        QStringLiteral("/nonexistent/definitely/missing/DevelopmentProcess/requirements"));
    QVERIFY(none.isEmpty());

    // Existing but empty directory -> empty result, no crash.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QVector<Requirement> empty = RequirementsParser::parseDirectory(temp.path());
    QVERIFY(empty.isEmpty());

    // Directory containing only non-requirement markdown -> empty result.
    writeFixture(temp.path(), QStringLiteral("README.md"),
                 QStringLiteral("# Just a README\n\nNo requirement template here.\n"));
    const QVector<Requirement> readmeOnly =
        RequirementsParser::parseDirectory(temp.path());
    QVERIFY(readmeOnly.isEmpty());
}

void TestParser::generateNextId_scansActiveAndArchive()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();
    const QString activeDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    const QString archiveDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/archive"));

    writeFixture(activeDir, QStringLiteral("REQ-SW-001.md"),
                 requirementContent(QStringLiteral("REQ-SW-001"),
                                    QStringLiteral("One"), QStringLiteral("ACTIVE"),
                                    QStringLiteral("—"), QStringLiteral("—")));
    writeFixture(activeDir, QStringLiteral("REQ-SW-002.md"),
                 requirementContent(QStringLiteral("REQ-SW-002"),
                                    QStringLiteral("Two"), QStringLiteral("ACTIVE"),
                                    QStringLiteral("—"), QStringLiteral("—")));
    writeFixture(activeDir, QStringLiteral("REQ-SW-003.md"),
                 requirementContent(QStringLiteral("REQ-SW-003"),
                                    QStringLiteral("Three"), QStringLiteral("ACTIVE"),
                                    QStringLiteral("—"), QStringLiteral("—")));
    writeFixture(activeDir, QStringLiteral("REQ-PLG-005.md"),
                 requirementContent(QStringLiteral("REQ-PLG-005"),
                                    QStringLiteral("Plugin Five"), QStringLiteral("ACTIVE"),
                                    QStringLiteral("—"), QStringLiteral("—")));

    QCOMPARE(RequirementsParser::generateNextId(baseDir, QStringLiteral("SW")),
             QStringLiteral("REQ-SW-004"));
    QCOMPARE(RequirementsParser::generateNextId(baseDir, QStringLiteral("PLG")),
             QStringLiteral("REQ-PLG-006"));

    // A requirement in archive/ must also be considered (active AND archive).
    writeFixture(archiveDir, QStringLiteral("REQ-SW-010-archived.md"),
                 requirementContent(QStringLiteral("REQ-SW-010"),
                                    QStringLiteral("Ten"), QStringLiteral("DONE"),
                                    QStringLiteral("—"), QStringLiteral("—")));
    QCOMPARE(RequirementsParser::generateNextId(baseDir, QStringLiteral("SW")),
             QStringLiteral("REQ-SW-011"));
}

void TestParser::moveToArchive_movesFile()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();
    const QString activeDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));

    writeFixture(activeDir, QStringLiteral("REQ-SW-001.md"),
                 requirementContent(QStringLiteral("REQ-SW-001"),
                                    QStringLiteral("One"), QStringLiteral("ACTIVE"),
                                    QStringLiteral("—"), QStringLiteral("—")));

    const QVector<Requirement> before = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(before.size(), 1);
    QCOMPARE(before.first().section, QStringLiteral("active"));

    QVERIFY(RequirementsParser::moveToArchive(before.first().filePath));

    QVERIFY(!QFile::exists(before.first().filePath));
    const QString archiveDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/archive"));
    QVERIFY(QFile::exists(QDir(archiveDir).filePath(QStringLiteral("REQ-SW-001.md"))));

    const QVector<Requirement> after = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(after.size(), 1);
    QCOMPARE(after.first().section, QStringLiteral("archive"));
    QVERIFY(after.first().filePath.contains(QStringLiteral("/archive/")));
}

void TestParser::moveToActive_movesFile()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();
    const QString archiveDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/archive"));

    writeFixture(archiveDir, QStringLiteral("REQ-SW-001.md"),
                 requirementContent(QStringLiteral("REQ-SW-001"),
                                    QStringLiteral("One"), QStringLiteral("DONE"),
                                    QStringLiteral("—"), QStringLiteral("—")));

    const QVector<Requirement> before = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(before.size(), 1);
    QCOMPARE(before.first().section, QStringLiteral("archive"));

    QVERIFY(RequirementsParser::moveToActive(before.first().filePath));

    QVERIFY(!QFile::exists(before.first().filePath));
    const QString activeDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    QVERIFY(QFile::exists(QDir(activeDir).filePath(QStringLiteral("REQ-SW-001.md"))));

    const QVector<Requirement> after = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(after.size(), 1);
    QCOMPARE(after.first().section, QStringLiteral("active"));
}

void TestParser::moveRejectsWrongSection()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();
    const QString activeDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    const QString archiveDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/archive"));

    writeFixture(activeDir, QStringLiteral("REQ-SW-001.md"),
                 requirementContent(QStringLiteral("REQ-SW-001"),
                                    QStringLiteral("One"), QStringLiteral("ACTIVE"),
                                    QStringLiteral("—"), QStringLiteral("—")));
    writeFixture(archiveDir, QStringLiteral("REQ-SW-002.md"),
                 requirementContent(QStringLiteral("REQ-SW-002"),
                                    QStringLiteral("Two"), QStringLiteral("DONE"),
                                    QStringLiteral("—"), QStringLiteral("—")));
    writeFixture(temp.path(), QStringLiteral("loose.md"),
                 requirementContent(QStringLiteral("REQ-SW-003"),
                                    QStringLiteral("Three"), QStringLiteral("ACTIVE"),
                                    QStringLiteral("—"), QStringLiteral("—")));

    const QVector<Requirement> requirements = RequirementsParser::parseDirectory(baseDir);

    // Moving an active file to archive is valid, moving an archive file to
    // archive again is not.
    const Requirement *req1 = findRequirement(requirements, QStringLiteral("REQ-SW-001"));
    const Requirement *req2 = findRequirement(requirements, QStringLiteral("REQ-SW-002"));
    QVERIFY(req1);
    QVERIFY(req2);

    QVERIFY(!RequirementsParser::moveToArchive(req2->filePath));
    QVERIFY(!RequirementsParser::moveToActive(req1->filePath));

    // A file outside active/ and archive/ belongs to no section.
    const QString loosePath = QDir(temp.path()).filePath(QStringLiteral("loose.md"));
    QVERIFY(!RequirementsParser::moveToArchive(loosePath));
    QVERIFY(!RequirementsParser::moveToActive(loosePath));
}

void TestParser::writeRequirement_roundTrip()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();
    const QString activeDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));

    writeFixture(activeDir, QStringLiteral("REQ-SW-001.md"),
                 requirementContent(QStringLiteral("REQ-SW-001"),
                                    QStringLiteral("One"), QStringLiteral("ACTIVE"),
                                    QStringLiteral("—"), QStringLiteral("—")));

    const QVector<Requirement> parsed = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(parsed.size(), 1);
    Requirement req = parsed.first();

    // 1. Status change round-trip.
    RequirementsParser::setStatusLine(req, QStringLiteral("DONE"));
    QVERIFY(RequirementsParser::writeRequirement(req));

    QVector<Requirement> reparsed = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(reparsed.size(), 1);
    QCOMPARE(reparsed.first().status, QStringLiteral("DONE"));
    QVERIFY(reparsed.first().rawContent.contains(QStringLiteral("- **Статус:** DONE")));

    // 2. Acceptance criterion toggle round-trip (unchecked -> checked).
    req = reparsed.first();
    QVERIFY(!req.criteriaDone.isEmpty());
    QVERIFY(!req.criteriaDone.at(0));
    RequirementsParser::setCriterionChecked(req, 0, true);
    QVERIFY(RequirementsParser::writeRequirement(req));

    reparsed = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(reparsed.size(), 1);
    QVERIFY(reparsed.first().criteriaDone.at(0));
    QVERIFY(reparsed.first().rawContent.contains(QStringLiteral("- [x] 1. Критерий.")));

    // 3. Dependency list round-trip.
    req = reparsed.first();
    RequirementsParser::setDependenciesLine(req, {QStringLiteral("REQ-SW-002"),
                                                  QStringLiteral("REQ-SW-003")});
    QVERIFY(RequirementsParser::writeRequirement(req));

    reparsed = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(reparsed.size(), 1);
    QCOMPARE(reparsed.first().dependencies.size(), 2);
    QCOMPARE(reparsed.first().dependencies.at(0), QStringLiteral("REQ-SW-002"));
    QCOMPARE(reparsed.first().dependencies.at(1), QStringLiteral("REQ-SW-003"));

    // 4. Clearing the dependency list writes "—".
    req = reparsed.first();
    RequirementsParser::setDependenciesLine(req, {});
    QVERIFY(RequirementsParser::writeRequirement(req));

    reparsed = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(reparsed.size(), 1);
    QVERIFY(reparsed.first().dependencies.isEmpty());
    QVERIFY(reparsed.first().rawContent.contains(QStringLiteral("- **Зависи от:** —")));
}

// No QTEST_GUILESS_MAIN here: the three test classes share one binary whose
// main lives in test_main.cpp.
