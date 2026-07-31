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
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-001"), QStringLiteral("active"), QString()));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-002"), QStringLiteral("active"), QStringLiteral("REQ-SW-001")));

    const QVector<RequirementsValidator::Issue> issues = RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 0);
}

void TestValidator::danglingParent_error()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-001"), QStringLiteral("active"), QString()));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-002"), QStringLiteral("active"), QStringLiteral("REQ-SW-999")));

    const QVector<RequirementsValidator::Issue> issues = RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Error);
    QCOMPARE(issues.at(0).field, QStringLiteral("parentId"));
    QVERIFY(issues.at(0).message.contains(QStringLiteral("does not exist")));
}

void TestValidator::danglingDependency_error()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-001"), QStringLiteral("active"), QString()));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-002"), QStringLiteral("active"), QString(),
                                {QStringLiteral("REQ-SW-999")}));

    const QVector<RequirementsValidator::Issue> issues = RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Error);
    QCOMPARE(issues.at(0).field, QStringLiteral("dependencies"));
    QVERIFY(issues.at(0).message.contains(QStringLiteral("does not exist")));
}

void TestValidator::cycle_detection()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-001"), QStringLiteral("active"),
                                QStringLiteral("REQ-SW-002")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-002"), QStringLiteral("active"),
                                QStringLiteral("REQ-SW-001")));

    const QVector<RequirementsValidator::Issue> issues = RequirementsValidator::validate(reqs);

    // A 2-node parent cycle produces exactly one reported chain
    // (REQ-SW-001 -> REQ-SW-002 -> REQ-SW-001).
    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Error);
    QCOMPARE(issues.at(0).field, QStringLiteral("parentId"));
    QVERIFY(issues.at(0).message.contains(QStringLiteral("cycle detected")));
}

void TestValidator::missingFields_warnings()
{
    QVector<Requirement> reqs;
    Requirement req;
    req.id = QStringLiteral("REQ-SW-001");
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
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-001"), QStringLiteral("active"), QString(),
                                {QStringLiteral("REQ-SW-001")}));

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
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-001"), QStringLiteral("archive"), QString()));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-002"), QStringLiteral("active"), QString(),
                                {QStringLiteral("REQ-SW-001")}));

    const QVector<RequirementsValidator::Issue> issues = RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Warning);
    QCOMPARE(issues.at(0).field, QStringLiteral("dependencies"));
    QVERIFY(issues.at(0).message.contains(QStringLiteral("archived requirement")));
}

// No QTEST_GUILESS_MAIN here: the three test classes share one binary whose
// main lives in test_main.cpp.