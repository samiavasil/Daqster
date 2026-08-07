#include <QtTest>

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

// No QTEST_GUILESS_MAIN here: the three test classes share one binary whose
// main lives in test_main.cpp.