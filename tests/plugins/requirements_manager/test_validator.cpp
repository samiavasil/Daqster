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
                            const QStringList &dependencies = QStringList(),
                            const QString &status = QStringLiteral("ACTIVE"))
{
    Requirement req;
    req.id = id;
    req.title = id;
    req.status = status;
    req.priority = QStringLiteral("Medium");
    req.section = section;
    req.parentId = parentId;
    req.dependencies = dependencies;
    req.description = QStringLiteral("Description");
    req.date = QStringLiteral("2026-07-31");
    req.acceptanceCriteria << QStringLiteral("1. Criterion");
    req.criteriaDone.append(false); // parallel to acceptanceCriteria
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
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("несъществуващо изискване")),
             qPrintable(issues.at(0).message));
    QVERIFY(issues.at(0).message.contains(QStringLiteral("REQ-SW-PL-999")));
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
    // A live requirement depending on a CANCELLED one warns: the dependency
    // was dropped, so the dependent cannot be built on it. A DONE dependency
    // is the normal end state and stays silent (see
    // doneDependency_noWarning) — warning on it flagged the correct archiving
    // of a finished requirement.
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("archive"),
                                QString(), {}, QStringLiteral("CANCELLED")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active"), QString(),
                                {QStringLiteral("REQ-SW-PL-001")}));

    const QVector<RequirementsValidator::Issue> issues = RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Warning);
    QCOMPARE(issues.at(0).field, QStringLiteral("dependencies"));
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("отказано изискване")),
             qPrintable(issues.at(0).message));
    QVERIFY(issues.at(0).message.contains(QStringLiteral("REQ-SW-PL-001")));
}

void TestValidator::doneDependency_noWarning()
{
    QVector<Requirement> reqs;
    // Archiving a DONE requirement is what the lifecycle asks for, so a live
    // requirement depending on it must not be flagged.
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("archive"),
                                QString(), {}, QStringLiteral("DONE")));
    reqs[0].acceptanceCriteria = QStringList{QStringLiteral("criterion")};
    reqs[0].criteriaDone = QVector<bool>{true};
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active"), QString(),
                                {QStringLiteral("REQ-SW-PL-001")}));

    const QVector<RequirementsValidator::Issue> issues = RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 0);
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

// Writes one requirement .md using the conventional template. @p criterionDone
// writes "- [x]" instead of "- [ ]" — a DONE requirement may not keep an
// unaddressed criterion (validator §4 parity with the CLI). @p descriptionHeading
// selects the template generation (canonical "## Накратко" by default; an empty
// string writes no description section at all).
QString writeRequirementFixture(const QString &dirPath, const QString &fileName,
                                const QString &id, const QString &status,
                                const QString &dependsOn,
                                bool criterionDone = false,
                                const QString &descriptionHeading =
                                    QStringLiteral("## Накратко"))
{
    QDir().mkpath(dirPath);
    const QString filePath = QDir(dirPath).filePath(fileName);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return QString();
    const QString description = descriptionHeading.isEmpty()
        ? QString()
        : QStringLiteral("%1\n\nFixture requirement.\n\n").arg(descriptionHeading);
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
        "%5"
        "## Acceptance Criteria\n"
        "\n"
        "- [%4] 1. Criterion.\n").arg(id, status, dependsOn,
                                      criterionDone ? QStringLiteral("x")
                                                    : QStringLiteral(" "),
                                      description);
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
    // archive/ is in scope as well. DONE there is a legal zone; the criterion
    // is closed so the DONE/AC invariant (CLI §4) stays satisfied.
    QVERIFY(!writeRequirementFixture(
                 QDir(requirementsDir).filePath(QStringLiteral("archive")),
                 QStringLiteral("REQ-SW-PL-002-fixture.md"),
                 QStringLiteral("REQ-SW-PL-002"), QStringLiteral("DONE"),
                 QStringLiteral("—"), true).isEmpty());
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
void TestValidator::statusZone_activeInArchive_error()
{
    // RDD-PROCESS: ACTIVE -> DONE, and DONE moves the file to archive/. An
    // ACTIVE requirement filed in archive/ contradicts its own status. The CLI
    // reports the same defect (§3 zone check) — this is the parity that makes
    // the two validators agree instead of each inventing an answer.
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"),
                                QStringLiteral("archive"), QString()));

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Error);
    QCOMPARE(issues.at(0).field, QStringLiteral("status"));
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("в archive/")),
             qPrintable(issues.at(0).message));
}

void TestValidator::statusZone_doneInActive_error()
{
    // The one real defect on the current tree (REQ-SW-PL-079): DONE while the
    // file still sits in active/.
    QVector<Requirement> reqs;
    Requirement req = makeRequirement(QStringLiteral("REQ-SW-PL-079"),
                                      QStringLiteral("active"), QString(), {},
                                      QStringLiteral("DONE"));
    req.criteriaDone[0] = true; // isolate the zone rule from the AC rule
    reqs.append(req);

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Error);
    QCOMPARE(issues.at(0).field, QStringLiteral("status"));
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("в active/")),
             qPrintable(issues.at(0).message));
}

void TestValidator::statusZone_implementedInArchive_noIssue()
{
    // IMPLEMENTED belongs to neither zone: the code is on a branch, so the file
    // may sit in active/ or be filed in archive/ once its record is finished.
    QVector<Requirement> archive;
    archive.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"),
                                   QStringLiteral("archive"), QString(), {},
                                   QStringLiteral("IMPLEMENTED")));
    QCOMPARE(RequirementsValidator::validate(archive).size(), 0);

    QVector<Requirement> active;
    active.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"),
                                  QStringLiteral("active"), QString(), {},
                                  QStringLiteral("IMPLEMENTED")));
    QCOMPARE(RequirementsValidator::validate(active).size(), 0);
}

void TestValidator::doneWithOpenCriterion_error()
{
    // DONE asserts the work landed, so an unaddressed criterion forbids it.
    // Mirrors CLI §4 (list convention).
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"),
                                QStringLiteral("archive"), QString(), {},
                                QStringLiteral("DONE")));

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Error);
    QCOMPARE(issues.at(0).field, QStringLiteral("acceptanceCriteria"));
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("незакрити AC")),
             qPrintable(issues.at(0).message));
}

void TestValidator::doneWithDeferredCriterion_warning()
{
    // An unchecked criterion carrying ⏸ (blocked by environment) or ⛔ (proven
    // blocker) is a documented deferral, not an unaddressed one: a Warning, and
    // the decision stays with a human.
    QVector<Requirement> reqs;
    Requirement req = makeRequirement(QStringLiteral("REQ-SW-PL-001"),
                                      QStringLiteral("archive"), QString(), {},
                                      QStringLiteral("DONE"));
    req.acceptanceCriteria[0] = QStringLiteral("1. Нещо ⏸ (чака среда).");
    reqs.append(req);

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Warning);
    QCOMPARE(issues.at(0).field, QStringLiteral("acceptanceCriteria"));
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("CND-046")),
             qPrintable(issues.at(0).message));
}

void TestValidator::nonRequirementParent_warning()
{
    // "Родител: ADR-AI-013" cites a decision record, not a requirement. The
    // reference is kept and reported as "not a requirement" — an Error would
    // claim the ADR is missing, which is false.
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-AI-021"),
                                QStringLiteral("active"),
                                QStringLiteral("ADR-AI-013")));

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Warning);
    QCOMPARE(issues.at(0).field, QStringLiteral("parentId"));
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("не е изискване")),
             qPrintable(issues.at(0).message));
}

void TestValidator::multipleParents_secondDangling_error()
{
    // Every listed parent is resolved, not only the first: a stale second
    // parent must not hide behind the one the tree displays.
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-AI-011"), QStringLiteral("active"),
                                QString()));
    Requirement child = makeRequirement(QStringLiteral("REQ-AI-013"),
                                        QStringLiteral("active"),
                                        QStringLiteral("REQ-AI-011"));
    child.parentIds.append(QStringLiteral("REQ-AI-011"));
    child.parentIds.append(QStringLiteral("REQ-AI-999"));
    reqs.append(child);

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Error);
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("REQ-AI-999")),
             qPrintable(issues.at(0).message));
}

// No QTEST_GUILESS_MAIN here: the three test classes share one binary whose
// main lives in test_main.cpp.

namespace {

// Parses and validates one fixture written with the given description heading,
// so the template generation comes from a real .md instead of a hand-built
// record (the validator grades parsed files only).
QVector<RequirementsValidator::Issue>
validateFixture(const QString &id, const QString &descriptionHeading,
                QString *error)
{
    QTemporaryDir temp;
    if (!temp.isValid()) {
        if (error)
            *error = QStringLiteral("temporary dir");
        return {};
    }
    const QString requirementsDir = QDir(temp.path()).filePath(
        QString::fromUtf8(kRequirementsSubdir));
    const QString path = writeRequirementFixture(
        QDir(requirementsDir).filePath(QStringLiteral("active")),
        QStringLiteral("%1-fixture.md").arg(id), id, QStringLiteral("ACTIVE"),
        QStringLiteral("—"), false, descriptionHeading);
    if (path.isEmpty()) {
        if (error)
            *error = QStringLiteral("fixture write");
        return {};
    }
    const QVector<Requirement> parsed = RequirementsParser::parseDirectory(temp.path());
    if (parsed.size() != 1) {
        if (error)
            *error = QStringLiteral("parsed %1 records").arg(parsed.size());
        return {};
    }
    return RequirementsValidator::validate(parsed);
}

} // namespace

void TestValidator::templateV2_canonical_noIssue()
{
    // "## Накратко" is the canonical generation: it must produce nothing at all.
    QString error;
    const QVector<RequirementsValidator::Issue> issues =
        validateFixture(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("## Накратко"),
                        &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(issues.size(), 0);
}

void TestValidator::templateV1_deviationWarning()
{
    // "## Описание" (v1) is recognised but not canonical: one Warning, never an
    // Error — the description itself is present and usable.
    QString error;
    const QVector<RequirementsValidator::Issue> issues =
        validateFixture(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("## Описание"),
                        &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Warning);
    QCOMPARE(issues.at(0).field, QStringLiteral("template"));
    QVERIFY2(issues.at(0).message.contains(
                 QStringLiteral("шаблон v1 (## Описание) — каноничният е v2 (## Накратко)")),
             qPrintable(issues.at(0).message));
}

void TestValidator::templateV0_deviationWarning()
{
    // "## Проблем" is the oldest generation (v0). Same grading: a Warning that
    // names the variant and the heading the file actually uses.
    QString error;
    const QVector<RequirementsValidator::Issue> issues =
        validateFixture(QStringLiteral("REQ-SW-PL-003"), QStringLiteral("## Проблем"),
                        &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Warning);
    QCOMPARE(issues.at(0).field, QStringLiteral("template"));
    QVERIFY2(issues.at(0).message.contains(
                 QStringLiteral("шаблон v0 (## Проблем) — каноничният е v2 (## Накратко)")),
             qPrintable(issues.at(0).message));
}

void TestValidator::templateMissing_descriptionError()
{
    // No description heading at all is not a deviation — the file follows no
    // template. That stays an Error: silencing it is how 60 undescribed
    // requirements passed as "recognised dialect".
    QString error;
    const QVector<RequirementsValidator::Issue> issues =
        validateFixture(QStringLiteral("REQ-SW-PL-004"), QString(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    int templateErrors = 0;
    for (const auto &issue : issues) {
        if (issue.field == QStringLiteral("template")) {
            ++templateErrors;
            QCOMPARE(issue.severity, RequirementsValidator::Severity::Error);
            QVERIFY2(issue.message.contains(QStringLiteral("## Накратко")),
                     qPrintable(issue.message));
        }
    }
    QCOMPARE(templateErrors, 1);
}

void TestValidator::dependencyOnImplemented_silent()
{
    // IMPLEMENTED means "the code is on a branch, nothing in develop yet" — a
    // real state, not a defect. A dependency on it is silent, like DONE.
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"),
                                QStringLiteral("archive"), QString(), {},
                                QStringLiteral("IMPLEMENTED")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"),
                                QStringLiteral("active"), QString(),
                                {QStringLiteral("REQ-SW-PL-001")}));

    QCOMPARE(RequirementsValidator::validate(reqs).size(), 0);
}

void TestValidator::dependencyOnArchivedUnknownStatus_warning()
{
    // The target exists but sits in archive/ with a status the vocabulary cannot
    // read: the reference resolves to a record nobody can grade, so the Warning
    // names the file to open instead of staying silent.
    QVector<Requirement> reqs;
    Requirement target = makeRequirement(QStringLiteral("REQ-SW-PL-001"),
                                         QStringLiteral("archive"), QString(), {},
                                         QStringLiteral("ОСТАРЯЛО"));
    target.fileName = QStringLiteral("REQ-SW-PL-001-old.md");
    reqs.append(target);
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"),
                                QStringLiteral("active"), QString(),
                                {QStringLiteral("REQ-SW-PL-001")}));

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(reqs);

    // The target's own status is reported too (invalid status, Error); this
    // test is about the reference-level Warning.
    int dependencyWarnings = 0;
    for (const auto &issue : issues) {
        if (issue.field == QStringLiteral("dependencies")
            && issue.severity == RequirementsValidator::Severity::Warning) {
            ++dependencyWarnings;
            QVERIFY2(issue.message.contains(QStringLiteral("неразпознат статус")),
                     qPrintable(issue.message));
            QVERIFY2(issue.message.contains(QStringLiteral("REQ-SW-PL-001-old.md")),
                     qPrintable(issue.message));
        }
    }
    QCOMPARE(dependencyWarnings, 1);
}

void TestValidator::childDangling_error()
{
    // "Деца:" is the inverse of "Родител:" and nothing else reads it, so a stale
    // child link must be an Error like any other broken reference.
    QVector<Requirement> reqs;
    Requirement parent = makeRequirement(QStringLiteral("REQ-SW-PL-001"),
                                         QStringLiteral("active"), QString());
    parent.childIds.append(QStringLiteral("REQ-SW-PL-999"));
    reqs.append(parent);

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Error);
    QCOMPARE(issues.at(0).field, QStringLiteral("childIds"));
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("REQ-SW-PL-999")),
             qPrintable(issues.at(0).message));
}
