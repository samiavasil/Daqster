#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "RequirementsValidator.h"
#include "RequirementsParser.h"
#include "test_validator.h"

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
    req.description = QStringLiteral("Description");
    req.date = QStringLiteral("2026-07-31");
    req.acceptanceCriteria << QStringLiteral("1. Criterion");
    return req;
}

} // namespace

void TestValidator::cleanFixture_noIssues()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active"), QString()));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active"), QStringLiteral("REQ-SW-PL-001")));

    const QVector<RequirementsValidator::Issue> issues = RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 0);
}

void TestValidator::danglingParent_error()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active"), QString()));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active"), QStringLiteral("REQ-SW-PL-999")));

    const QVector<RequirementsValidator::Issue> issues = RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Error);
    QCOMPARE(issues.at(0).field, QStringLiteral("parentId"));
    QVERIFY(issues.at(0).message.contains(QStringLiteral("does not exist")));
}

void TestValidator::danglingDependency_error()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active"), QString()));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active"), QString(),
                                {QStringLiteral("REQ-SW-PL-999")}));

    const QVector<RequirementsValidator::Issue> issues = RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Error);
    QCOMPARE(issues.at(0).field, QStringLiteral("dependencies"));
    QVERIFY(issues.at(0).message.contains(QStringLiteral("does not exist")));
}

void TestValidator::cycle_detection()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active"),
                                QStringLiteral("REQ-SW-PL-002")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active"),
                                QStringLiteral("REQ-SW-PL-001")));

    const QVector<RequirementsValidator::Issue> issues = RequirementsValidator::validate(reqs);

    // A 2-node parent cycle produces exactly one reported chain
    // (REQ-SW-PL-001 -> REQ-SW-PL-002 -> REQ-SW-PL-001).
    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Error);
    QCOMPARE(issues.at(0).field, QStringLiteral("parentId"));
    QVERIFY(issues.at(0).message.contains(QStringLiteral("cycle detected")));
}

void TestValidator::missingFields_warnings()
{
    QVector<Requirement> reqs;
    Requirement req;
    req.id = QStringLiteral("REQ-SW-PL-001");
    req.section = QStringLiteral("active");
    // Leave all fields empty except id and section

    reqs.append(req);

    const QVector<RequirementsValidator::Issue> issues = RequirementsValidator::validate(reqs);

    // Should have warnings for missing title, status, priority, date, description, acceptanceCriteria
    int warningCount = 0;
    for (const auto &issue : issues) {
        if (issue.severity == RequirementsValidator::Severity::Warning) {
            warningCount++;
        }
    }
    QVERIFY(warningCount >= 6);
}

void TestValidator::selfReference_error()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active"), QString(),
                                {QStringLiteral("REQ-SW-PL-001")}));

    const QVector<RequirementsValidator::Issue> issues = RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Error);
    QCOMPARE(issues.at(0).field, QStringLiteral("dependencies"));
    QVERIFY(issues.at(0).message.contains(QStringLiteral("depends on itself")));
}

void TestValidator::archivedDependency_warning()
{
    QVector<Requirement> reqs;
    // An active requirement depending on an archived one triggers the warning
    // ("depends on archived requirement"). The original fixture had the
    // sections inverted (archive depending on active), which correctly
    // produced no issue.
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("archive"), QString()));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active"), QString(),
                                {QStringLiteral("REQ-SW-PL-001")}));

    const QVector<RequirementsValidator::Issue> issues = RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Warning);
    QCOMPARE(issues.at(0).field, QStringLiteral("dependencies"));
    QVERIFY(issues.at(0).message.contains(QStringLiteral("archived requirement")));
}

void TestValidator::typedIdFormat_validAndMalformed()
{
    // A valid typed 4-segment ID (and the 3-segment private form) pass the
    // ID format check without any Warning.
    QVector<Requirement> valid;
    valid.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active"), QString()));
    valid.append(makeRequirement(QStringLiteral("REQ-PLG-001"), QStringLiteral("active"), QString()));
    const QVector<RequirementsValidator::Issue> validIssues =
        RequirementsValidator::validate(valid);
    QCOMPARE(validIssues.size(), 0);

    // Malformed IDs produce a format Warning on the "id" field with the
    // "не следва схемата" message.
    QVector<Requirement> malformed;
    malformed.append(makeRequirement(QStringLiteral("BOGUS"), QStringLiteral("active"), QString()));
    malformed.append(makeRequirement(QStringLiteral("REQ-SW-001-extra"), QStringLiteral("active"), QString()));
    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(malformed);

    QCOMPARE(issues.size(), 2);
    for (const auto &issue : issues) {
        QCOMPARE(issue.severity, RequirementsValidator::Severity::Warning);
        QCOMPARE(issue.field, QStringLiteral("id"));
        QVERIFY2(issue.message.contains(QStringLiteral("не следва схемата")),
                 qPrintable(issue.message));
    }
}

void TestValidator::twoDigitId_warning()
{
    // The typed scheme requires a zero-padded three-digit number: 4-segment
    // "REQ-SW-PL-001" and 3-segment "REQ-PLG-001" pass, but 2-digit forms such
    // as "REQ-SW-12" / "REQ-PLG-01" are rejected with an ID format Warning.
    QVector<Requirement> twoDigit;
    twoDigit.append(makeRequirement(QStringLiteral("REQ-SW-12"), QStringLiteral("active"), QString()));
    twoDigit.append(makeRequirement(QStringLiteral("REQ-PLG-01"), QStringLiteral("active"), QString()));
    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(twoDigit);

    QCOMPARE(issues.size(), 2);
    for (const auto &issue : issues) {
        QCOMPARE(issue.severity, RequirementsValidator::Severity::Warning);
        QCOMPARE(issue.field, QStringLiteral("id"));
        QVERIFY2(issue.message.contains(QStringLiteral("не следва схемата")),
                 qPrintable(issue.message));
    }
}

void TestValidator::duplicateId_acrossRepos_error()
{
    // With a merged multi-repo vector the same bare ID can appear twice (e.g.
    // a public REQ-SW-PL-013 and a private duplicate). Every duplicate
    // occurrence after the first is an Error on the "id" field.
    QVector<Requirement> reqs;
    Requirement publicReq = makeRequirement(QStringLiteral("REQ-SW-PL-013"),
                                            QStringLiteral("active"), QString());
    publicReq.repo = QStringLiteral("public");
    reqs.append(publicReq);
    Requirement privateReq = makeRequirement(QStringLiteral("REQ-SW-PL-013"),
                                             QStringLiteral("active"), QString());
    privateReq.repo = QStringLiteral("private");
    reqs.append(privateReq);

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Error);
    QCOMPARE(issues.at(0).field, QStringLiteral("id"));
    QVERIFY(issues.at(0).message.contains(QStringLiteral("duplicate requirement ID")));
    QVERIFY(issues.at(0).message.contains(QStringLiteral("public")));
    QVERIFY(issues.at(0).message.contains(QStringLiteral("private")));
}

void TestValidator::crossRepoParent_notDangling()
{
    // A public requirement whose parent lives in the private repo resolves via
    // the lowercase findById lookup in the merged vector - no dangling error.
    QVector<Requirement> reqs;
    Requirement child = makeRequirement(QStringLiteral("REQ-SW-PL-001"),
                                        QStringLiteral("active"),
                                        QStringLiteral("REQ-PLG-004"));
    child.repo = QStringLiteral("public");
    reqs.append(child);
    Requirement parent = makeRequirement(QStringLiteral("REQ-PLG-004"),
                                         QStringLiteral("active"), QString());
    parent.repo = QStringLiteral("private");
    reqs.append(parent);

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 0);
}

void TestValidator::hintMismatch_warning()
{
    // The annotation "частно" claims the referenced requirement lives in the
    // private repo, but it resolves to a public one -> Warning.
    QVector<Requirement> reqs;
    Requirement source = makeRequirement(QStringLiteral("REQ-SW-PL-012"),
                                         QStringLiteral("active"), QString(),
                                         {QStringLiteral("REQ-SW-PL-013")});
    source.repo = QStringLiteral("public");
    source.dependencyHints.insert(QStringLiteral("REQ-SW-PL-013"),
                                  QStringLiteral("частно"));
    reqs.append(source);
    Requirement target = makeRequirement(QStringLiteral("REQ-SW-PL-013"),
                                         QStringLiteral("active"), QString());
    target.repo = QStringLiteral("public");
    reqs.append(target);

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Warning);
    QVERIFY(issues.at(0).message.contains(QStringLiteral("annotated as")));
    QVERIFY(issues.at(0).message.contains(QStringLiteral("resolves to")));
}

void TestValidator::hintMatch_noWarning()
{
    // "публично" hint against a public target matches -> no hint Warning.
    QVector<Requirement> reqs;
    Requirement source = makeRequirement(QStringLiteral("REQ-SW-PL-012"),
                                         QStringLiteral("active"), QString(),
                                         {QStringLiteral("REQ-SW-PL-013")});
    source.repo = QStringLiteral("public");
    source.dependencyHints.insert(QStringLiteral("REQ-SW-PL-013"),
                                  QStringLiteral("публично"));
    reqs.append(source);
    Requirement target = makeRequirement(QStringLiteral("REQ-SW-PL-013"),
                                         QStringLiteral("active"), QString());
    target.repo = QStringLiteral("public");
    reqs.append(target);

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 0);
}

namespace {

// Writes one requirement .md using the conventional template.
QString writeRequirementFixture(const QString &dirPath, const QString &fileName,
                                const QString &id, const QString &status,
                                const QString &dependsOn)
{
    QDir().mkpath(dirPath);
    const QString filePath = QDir(dirPath).filePath(fileName);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return QString();
    const QString content = QStringLiteral(
        "# %1: Fixture\n"
        "\n"
        "- **Статус:** %2\n"
        "- **Приоритет:** Medium\n"
        "- **Отговорник (роля):** Implementation\n"
        "- **Дата:** 2026-11-16\n"
        "- **Родител:** —\n"
        "- **Зависи от:** %3\n"
        "\n"
        "## Описание\n"
        "\n"
        "Fixture requirement.\n"
        "\n"
        "## Acceptance Criteria\n"
        "\n"
        "- [ ] 1. Criterion.\n").arg(id, status, dependsOn);
    file.write(content.toUtf8());
    file.close();
    return filePath;
}

} // namespace

void TestValidator::scope_onlyRequirementsTreeIsValidated()
{
    // A repo root whose plans/ tree holds a requirement-shaped draft. The real
    // tree has 9 DRAFT-PL/FW files under plans/ideas/ plus their archive/
    // copies and one full REQ-SW-PL-050 copy in plans/ — walking the repo root
    // turned those into requirements (duplicate IDs, malformed IDs, missing
    // fields), which is where the GUI's 47 errors came from while the CLI
    // reported 2. The parse scope is the requirements tree only.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString root = temp.path();
    const QString requirementsDir = QDir(root).filePath(
        QString::fromUtf8(kRequirementsSubdir));

    QVERIFY(!writeRequirementFixture(
                 QDir(requirementsDir).filePath(QStringLiteral("active")),
                 QStringLiteral("REQ-SW-PL-001-fixture.md"),
                 QStringLiteral("REQ-SW-PL-001"), QStringLiteral("ACTIVE"),
                 QStringLiteral("—")).isEmpty());
    // archive/ is in scope as well.
    QVERIFY(!writeRequirementFixture(
                 QDir(requirementsDir).filePath(QStringLiteral("archive")),
                 QStringLiteral("REQ-SW-PL-002-fixture.md"),
                 QStringLiteral("REQ-SW-PL-002"), QStringLiteral("DONE"),
                 QStringLiteral("—")).isEmpty());
    // Out of scope: a draft with an invalid status AND a dangling dependency.
    // Both would be Errors if it were parsed.
    QVERIFY(!writeRequirementFixture(
                 QDir(root).filePath(QStringLiteral("DevelopmentProcess/plans")),
                 QStringLiteral("REQ-SW-PL-777-draft.md"),
                 QStringLiteral("REQ-SW-PL-777"), QStringLiteral("PLANNED"),
                 QStringLiteral("REQ-SW-PL-998")).isEmpty());

    const QVector<Requirement> parsed = RequirementsParser::parseDirectory(root);

    QCOMPARE(parsed.size(), 2);
    QCOMPARE(parsed.at(0).id, QStringLiteral("REQ-SW-PL-001"));
    QCOMPARE(parsed.at(1).id, QStringLiteral("REQ-SW-PL-002"));

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(parsed);

    // The draft would have reported "невалиден статус" and a dangling
    // dependency; nothing from plans/ reaches the validator.
    QCOMPARE(issues.size(), 0);
}

void TestValidator::statusVocabulary_hasOneEntryPoint()
{
    QCOMPARE(RequirementsValidator::statusVocabulary().join(QLatin1Char(' ')),
             QStringLiteral("ACTIVE IMPLEMENTED DONE CANCELLED BACKLOG"));
}

void TestValidator::statusVocabulary_invalidStatus_error()
{
    QVector<Requirement> reqs;
    Requirement req = makeRequirement(QStringLiteral("REQ-SW-PL-001"),
                                      QStringLiteral("active"), QString());
    req.status = QStringLiteral("PLANNED");
    reqs.append(req);

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Error);
    QCOMPARE(issues.at(0).field, QStringLiteral("status"));
    QVERIFY(issues.at(0).message.contains(QStringLiteral("PLANNED")));
    QVERIFY(issues.at(0).message.contains(QStringLiteral("BACKLOG")));
}

void TestValidator::statusVocabulary_doneNaKlonaMapsToImplemented()
{
    // "DONE НА КЛОНА" is IMPLEMENTED, not DONE: the code is on a branch.
    QCOMPARE(RequirementsValidator::normalizedStatus(
                 QStringLiteral("DONE НА КЛОНА (AC1–AC8) — чака merge в develop")),
             QStringLiteral("IMPLEMENTED"));
    // A rationale after the status never overrides it.
    QCOMPARE(RequirementsValidator::normalizedStatus(
                 QStringLiteral("ACTIVE — IN PROGRESS (2026-11-10: MVP)")),
             QStringLiteral("ACTIVE"));
    // Emphasis and backticks around the value are not part of the status.
    QCOMPARE(RequirementsValidator::normalizedStatus(QStringLiteral("**ACTIVE**")),
             QStringLiteral("ACTIVE"));
    QCOMPARE(RequirementsValidator::normalizedStatus(QStringLiteral("")), QString());

    QVector<Requirement> reqs;
    Requirement req = makeRequirement(QStringLiteral("REQ-SW-PL-001"),
                                      QStringLiteral("active"), QString());
    req.status = QStringLiteral("DONE НА КЛОНА (AC1–AC8) — чака merge в develop");
    reqs.append(req);

    QCOMPARE(RequirementsValidator::validate(reqs).size(), 0);
}

// No QTEST_GUILESS_MAIN here: the three test classes share one binary whose
// main lives in test_main.cpp.