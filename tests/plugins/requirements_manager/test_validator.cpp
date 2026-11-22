#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QProcess>

#include "RequirementsValidator.h"
#include "RequirementsParser.h"
#include "DoneGate.h"
#include "DoneGateCheck.h"
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

bool writeFileAt(const QString &root, const QString &relative, const QString &content)
{
    const QString path = QDir(root).filePath(relative);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    file.write(content.toUtf8());
    return true;
}

bool runGit(const QString &root, const QStringList &arguments)
{
    QProcess git;
    git.setWorkingDirectory(root);
    git.start(QStringLiteral("git"), arguments);
    if (!git.waitForStarted(5000) || !git.waitForFinished(20000))
        return false;
    return git.exitStatus() == QProcess::NormalExit && git.exitCode() == 0;
}

// A requirement that can be a DONE candidate, except for the one thing a test
// changes: the AC state, the review record, or where the code lives.
QString doneReadyMarkdown(const QString &id, const QString &status,
                          const QString &checked, const QString &codePath)
{
    return QStringLiteral(
               "# %1: Fixture\n"
               "\n"
               "- **Статус:** %2\n"
               "- **Приоритет:** High\n"
               "- **Отговорник (роля):** Implementation\n"
               "- **Дата:** 2026-11-22\n"
               "- **Родител:** —\n"
               "- **Зависи от:** —\n"
               "- **Ревизия:** 2\n"
               "\n"
               "## Накратко\n"
               "\n"
               "Фикстура.\n"
               "\n"
               "## Как ще разберем, че работи\n"
               "\n"
               "- [%3] AC1 — първи критерий\n"
               "\n"
               "## Проследимост\n"
               "\n"
               "- **Код:** `%4`\n")
        .arg(id, status, checked, codePath);
}

QString reviewMarkdown(const QString &id, const QString &verdict)
{
    return QStringLiteral(
               "# Ревю: feat/x @ abc123 (план: —, AC: 1, %1)\n"
               "\n"
               "## Кръгове\n"
               "\n"
               "| итерация | A | отворени | затворени | присъда |\n"
               "|----------|---|----------|-----------|---------|\n"
               "| R1 | 0 | — | — | %2 |\n")
        .arg(id, verdict);
}

} // namespace

void TestValidator::cleanFixture_noIssues(){
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
             QStringLiteral("DRAFT ACTIVE IMPLEMENTED DONE CANCELLED BACKLOG"));
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

void TestValidator::statusVocabulary_draftOnlyInActive()
{
    // DRAFT is the state an agent writes and the owner accepts in the Manager
    // (REQ-SW-PL-099). It is a legal status, and it is legal only in active/:
    // the approval happens there, so a draft in archive/ is a contradiction.
    QVector<Requirement> active;
    active.append(makeRequirement(QStringLiteral("REQ-SW-PL-096"),
                                  QStringLiteral("active"), QString(), {},
                                  QStringLiteral("DRAFT")));
    QCOMPARE(RequirementsValidator::validate(active).size(), 0);

    QVector<Requirement> archived;
    archived.append(makeRequirement(QStringLiteral("REQ-SW-PL-097"),
                                    QStringLiteral("archive"), QString(), {},
                                    QStringLiteral("DRAFT")));
    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(archived);
    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Error);
    QCOMPARE(issues.at(0).field, QStringLiteral("status"));
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("DRAFT"))
                 && issues.at(0).message.contains(QStringLiteral("в archive/")),
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

void TestValidator::dateNotIso_warning()
{
    // REQ-SW-PL-100: "Дата:" is a date, not free text. A sloppy value is a
    // Warning that names it — the file stays readable, but not invisible.
    QVector<Requirement> reqs;
    Requirement req = makeRequirement(QStringLiteral("REQ-SW-PL-001"),
                                      QStringLiteral("active"), QString());
    req.date = QStringLiteral("21.11.2026");
    reqs.append(req);

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Warning);
    QCOMPARE(issues.at(0).field, QStringLiteral("date"));
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("21.11.2026")),
             qPrintable(issues.at(0).message));
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("YYYY-MM-DD")),
             qPrintable(issues.at(0).message));
}

void TestValidator::dateMissing_warningStays()
{
    // An empty date keeps its own, older message: "missing" and "not a date"
    // are two different repairs, so they stay two different issues.
    QVector<Requirement> reqs;
    Requirement req = makeRequirement(QStringLiteral("REQ-SW-PL-001"),
                                      QStringLiteral("active"), QString());
    req.date = QString();
    reqs.append(req);

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).field, QStringLiteral("date"));
    QCOMPARE(issues.at(0).message, QStringLiteral("creation date is missing"));
}

// ── Cited code paths (REQ-SW-PL-103) ────────────────────────────────────────
// A path in "Код:" is a claim that the file exists — but only an implemented
// requirement makes that claim. An accepted one may cite a file it is about to
// add; a draft may cite a file nobody has planned yet.
void TestValidator::citedPath_activeMissingPath_warning()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Requirement req = makeRequirement(QStringLiteral("REQ-SW-PL-101"),
                                      QStringLiteral("active"), QString());
    req.status = QStringLiteral("ACTIVE");
    req.filePath = dir.filePath(QStringLiteral("DevelopmentProcess/requirements/active/x.md"));
    req.code = QStringLiteral("`scripts/check-req-history.sh` (още го няма)");

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validateCitedPaths({req}, {dir.path()});

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Warning);
    QCOMPARE(issues.at(0).field, QStringLiteral("code"));
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("check-req-history.sh")),
             qPrintable(issues.at(0).message));
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("още не е имплементиран")),
             qPrintable(issues.at(0).message));
}

void TestValidator::citedPath_implementedMissingPath_error()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Requirement req = makeRequirement(QStringLiteral("REQ-SW-PL-101"),
                                      QStringLiteral("active"), QString());
    req.status = QStringLiteral("IMPLEMENTED");
    req.filePath = dir.filePath(QStringLiteral("DevelopmentProcess/requirements/active/x.md"));
    req.code = QStringLiteral("`scripts/check-req-history.sh`");

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validateCitedPaths({req}, {dir.path()});

    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Error);
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("не съществува")),
             qPrintable(issues.at(0).message));

    // DONE is the same claim as IMPLEMENTED: the work landed in develop.
    req.status = QStringLiteral("DONE");
    const QVector<RequirementsValidator::Issue> done =
        RequirementsValidator::validateCitedPaths({req}, {dir.path()});
    QCOMPARE(done.size(), 1);
    QCOMPARE(done.at(0).severity, RequirementsValidator::Severity::Error);
}

void TestValidator::citedPath_draftAndExisting_silent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("scripts")));
    QFile stamp(dir.filePath(QStringLiteral("scripts/check-req-history.sh")));
    QVERIFY(stamp.open(QIODevice::WriteOnly));
    stamp.close();

    Requirement draft = makeRequirement(QStringLiteral("REQ-SW-PL-101"),
                                        QStringLiteral("active"), QString());
    draft.status = QStringLiteral("DRAFT");
    draft.filePath = dir.filePath(QStringLiteral("DevelopmentProcess/requirements/active/x.md"));
    draft.code = QStringLiteral("`scripts/does-not-exist.sh`");

    Requirement active = makeRequirement(QStringLiteral("REQ-SW-PL-102"),
                                         QStringLiteral("active"), QString());
    active.status = QStringLiteral("ACTIVE");
    active.filePath = dir.filePath(QStringLiteral("DevelopmentProcess/requirements/active/y.md"));
    active.code = QStringLiteral("`scripts/check-req-history.sh`");

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validateCitedPaths({draft, active}, {dir.path()});

    QCOMPARE(issues.size(), 0);

    // The severity rule itself is one function, so it cannot drift between
    // callers: ACTIVE warns, IMPLEMENTED/DONE error, DRAFT grades nothing.
    QCOMPARE(RequirementsValidator::pathSeverityForStatus(QString::fromLatin1("ACTIVE")).value(),
             RequirementsValidator::Severity::Warning);
    QCOMPARE(RequirementsValidator::pathSeverityForStatus(QString::fromLatin1("IMPLEMENTED")).value(),
             RequirementsValidator::Severity::Error);
    QVERIFY(!RequirementsValidator::pathSeverityForStatus(QString::fromLatin1("DRAFT")).has_value());
}

void TestValidator::createdField_isTheCreationDate()
{
    // REQ-SW-PL-101 AC2: "Създадено:" is the field, "Дата:" the legacy alias.
    // Neither present is "missing"; a sloppy value in the new field is the same
    // "не е YYYY-MM-DD" warning the old one gets.
    QVector<Requirement> reqs;
    Requirement neither = makeRequirement(QStringLiteral("REQ-SW-PL-001"),
                                          QStringLiteral("active"), QString());
    neither.date.clear();
    neither.created.clear();
    reqs.append(neither);

    Requirement fromCreated = makeRequirement(QStringLiteral("REQ-SW-PL-002"),
                                              QStringLiteral("active"), QString());
    fromCreated.date = QStringLiteral("не е дата");
    fromCreated.created = QStringLiteral("2026-11-21");
    reqs.append(fromCreated);

    Requirement sloppyCreated = makeRequirement(QStringLiteral("REQ-SW-PL-003"),
                                                QStringLiteral("active"), QString());
    sloppyCreated.date.clear();
    sloppyCreated.created = QStringLiteral("21.11.2026");
    reqs.append(sloppyCreated);

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validate(reqs);

    QCOMPARE(issues.size(), 2);
    QCOMPARE(issues.at(0).id, QStringLiteral("REQ-SW-PL-001"));
    QCOMPARE(issues.at(0).message, QStringLiteral("creation date is missing"));
    QCOMPARE(issues.at(1).id, QStringLiteral("REQ-SW-PL-003"));
    QVERIFY2(issues.at(1).message.contains(QStringLiteral("21.11.2026")),
             qPrintable(issues.at(1).message));
}

// ── Порта към DONE (REQ-SW-PL-107) ──────────────────────────────────────────

namespace {

// A temp repository whose `develop` carries @p onDevelop, then a work branch
// that carries @p onBranch (and nothing else new). Everything a gate test needs
// to tell "in develop" from "only on my branch" — with the real git.
// @p baseBranch names the integration branch this fixture has: `develop` unless
// a test needs a repo that has no such branch at all.
bool makeRepo(const QString &root, const QVector<QPair<QString, QString>> &onDevelop,
              const QVector<QPair<QString, QString>> &onBranch,
              const QString &baseBranch = QStringLiteral("develop"))
{
    if (!runGit(root, {QStringLiteral("init"), QStringLiteral("-q")}))
        return false;
    runGit(root, {QStringLiteral("config"), QStringLiteral("user.email"),
                  QStringLiteral("test@example.com")});
    runGit(root, {QStringLiteral("config"), QStringLiteral("user.name"),
                  QStringLiteral("Test")});
    if (!writeFileAt(root, QStringLiteral("README.md"), QStringLiteral("# fixture\n")))
        return false;
    for (const auto &entry : onDevelop)
        if (!writeFileAt(root, entry.first, entry.second))
            return false;
    if (!runGit(root, {QStringLiteral("add"), QStringLiteral("-A")}))
        return false;
    if (!runGit(root, {QStringLiteral("commit"), QStringLiteral("-q"),
                       QStringLiteral("-m"), QStringLiteral("base")}))
        return false;
    if (!runGit(root, {QStringLiteral("branch"), QStringLiteral("-M"),
                       baseBranch}))
        return false;
    if (onBranch.isEmpty())
        return true;
    if (!runGit(root, {QStringLiteral("checkout"), QStringLiteral("-q"),
                       QStringLiteral("-b"), QStringLiteral("feat/x")}))
        return false;
    for (const auto &entry : onBranch)
        if (!writeFileAt(root, entry.first, entry.second))
            return false;
    return runGit(root, {QStringLiteral("add"), QStringLiteral("-A")})
           && runGit(root, {QStringLiteral("commit"), QStringLiteral("-q"),
                            QStringLiteral("-m"), QStringLiteral("work")});
}

// The parsed fixture requirement of a temp repo (empty when parse failed).
Requirement parsedFixture(const QString &root, const QString &id)
{
    const QVector<Requirement> parsed = RequirementsParser::parseDirectory(root);
    for (const Requirement &req : parsed)
        if (req.id == id)
            return req;
    return Requirement();
}

// The DONE gate answers in two steps (REQ-SW-PL-107): the file half is
// synchronous, the `develop` half arrives from a child process — a wait inside
// the plugin is forbidden (check_no_event_loops.cmake). The test waits on its
// own event loop, exactly the way the widget does.
bool evaluateDoneGate(Requirement &req, const QVector<RequirementRoot> &roots)
{
    DoneGate::evaluateLocal(req, roots);
    DoneGateCheck check;
    QSignalSpy done(&check, &DoneGateCheck::finished);
    check.start(roots, {req});
    if (done.isEmpty() && !done.wait(15000))
        return false;
    const auto &answers = check.answers();
    const auto it = answers.constFind(req.filePath);
    if (it == answers.constEnd())
        return false;
    DoneGate::applyCodeAnswer(req, *it);
    return true;
}

constexpr char kReqPath[] =
    "DevelopmentProcess/requirements/active/REQ-SW-PL-107-fixture.md";
constexpr char kReviewPath[] = "DevelopmentProcess/plans/reviews/PR-1-REQ-SW-PL-107.md";

} // namespace

void TestValidator::doneGate_candidateWhenAllFourHold()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString root = temp.path();
    QVERIFY2(makeRepo(root,
                      {{QString::fromUtf8(kReqPath),
                        doneReadyMarkdown(QStringLiteral("REQ-SW-PL-107"),
                                          QStringLiteral("IMPLEMENTED"),
                                          QStringLiteral("x"),
                                          QStringLiteral("plugins/later.h"))},
                       {QStringLiteral("plugins/later.h"), QStringLiteral("// code\n")},
                       {QString::fromUtf8(kReviewPath),
                        reviewMarkdown(QStringLiteral("REQ-SW-PL-107"),
                                       QStringLiteral("ПРИЕТО"))}},
                      {}),
             "git fixture");

    Requirement req = parsedFixture(root, QStringLiteral("REQ-SW-PL-107"));
    QVERIFY(!req.id.isEmpty());
    QVERIFY(req.allCriteriaDone);   // parsed, not a line in the file

    QVERIFY2(evaluateDoneGate(req, {RequirementRoot{root}}),
             "the develop check must answer");
    QVERIFY(req.acceptingReview);
    QCOMPARE(req.reviewVerdict, QStringLiteral("ПРИЕТО"));
    QVERIFY(req.codeInDevelop);
    QVERIFY(req.missingDevelopPaths.isEmpty());
    QVERIFY(req.doneCandidate);
    QVERIFY(DoneGate::isDoneCandidate(req));
    QVERIFY(DoneGate::missingPreconditions(req).isEmpty());
}

void TestValidator::doneGate_notCandidateWhenCriterionOpen()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString root = temp.path();
    QVERIFY2(makeRepo(root,
                      {{QString::fromUtf8(kReqPath),
                        doneReadyMarkdown(QStringLiteral("REQ-SW-PL-107"),
                                          QStringLiteral("IMPLEMENTED"),
                                          QStringLiteral(" "),
                                          QStringLiteral("plugins/later.h"))},
                       {QStringLiteral("plugins/later.h"), QStringLiteral("// code\n")},
                       {QString::fromUtf8(kReviewPath),
                        reviewMarkdown(QStringLiteral("REQ-SW-PL-107"),
                                       QStringLiteral("ПРИЕТО"))}},
                      {}),
             "git fixture");

    Requirement req = parsedFixture(root, QStringLiteral("REQ-SW-PL-107"));
    QVERIFY2(evaluateDoneGate(req, {RequirementRoot{root}}),
             "the develop check must answer");

    QVERIFY(!req.allCriteriaDone);
    QVERIFY(!req.doneCandidate);
    const QStringList missing = DoneGate::missingPreconditions(req);
    QCOMPARE(missing.size(), 1);
    QVERIFY2(missing.first().contains(QStringLiteral("AC1")), qPrintable(missing.first()));
}

void TestValidator::doneGate_notCandidateWhenNoReviewRecord()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString root = temp.path();
    QVERIFY2(makeRepo(root,
                      {{QString::fromUtf8(kReqPath),
                        doneReadyMarkdown(QStringLiteral("REQ-SW-PL-107"),
                                          QStringLiteral("IMPLEMENTED"),
                                          QStringLiteral("x"),
                                          QStringLiteral("plugins/later.h"))},
                       {QStringLiteral("plugins/later.h"), QStringLiteral("// code\n")}},
                      {}),
             "git fixture");

    Requirement req = parsedFixture(root, QStringLiteral("REQ-SW-PL-107"));
    QVERIFY2(evaluateDoneGate(req, {RequirementRoot{root}}),
             "the develop check must answer");

    QVERIFY(req.reviewRecord.isEmpty());
    QVERIFY(!req.acceptingReview);
    QVERIFY(!req.doneCandidate);
    const QStringList missing = DoneGate::missingPreconditions(req);
    QCOMPARE(missing.size(), 1);
    QVERIFY2(missing.first().contains(QStringLiteral("ревю запис")), qPrintable(missing.first()));
}

void TestValidator::doneGate_notCandidateWhenVerdictReturned()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString root = temp.path();
    // A record that exists and is not accepting: "exists a file" is not a review.
    QVERIFY2(makeRepo(root,
                      {{QString::fromUtf8(kReqPath),
                        doneReadyMarkdown(QStringLiteral("REQ-SW-PL-107"),
                                          QStringLiteral("IMPLEMENTED"),
                                          QStringLiteral("x"),
                                          QStringLiteral("plugins/later.h"))},
                       {QStringLiteral("plugins/later.h"), QStringLiteral("// code\n")},
                       {QString::fromUtf8(kReviewPath),
                        reviewMarkdown(QStringLiteral("REQ-SW-PL-107"),
                                       QStringLiteral("ВЪРНАТО"))}},
                      {}),
             "git fixture");

    Requirement req = parsedFixture(root, QStringLiteral("REQ-SW-PL-107"));
    QVERIFY2(evaluateDoneGate(req, {RequirementRoot{root}}),
             "the develop check must answer");

    QVERIFY(!req.reviewRecord.isEmpty());
    QVERIFY(!req.acceptingReview);
    QCOMPARE(req.reviewVerdict, QStringLiteral("ВЪРНАТО"));
    QVERIFY(!req.doneCandidate);
    const QStringList missing = DoneGate::missingPreconditions(req);
    QCOMPARE(missing.size(), 1);
    QVERIFY2(missing.first().contains(QStringLiteral("не е приемане")),
             qPrintable(missing.first()));
}

void TestValidator::doneGate_notCandidateWhenCodeOnlyOnBranch()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString root = temp.path();
    QVERIFY2(makeRepo(root,
                      {{QString::fromUtf8(kReqPath),
                        doneReadyMarkdown(QStringLiteral("REQ-SW-PL-107"),
                                          QStringLiteral("IMPLEMENTED"),
                                          QStringLiteral("x"),
                                          QStringLiteral("plugins/later.h"))},
                       {QString::fromUtf8(kReviewPath),
                        reviewMarkdown(QStringLiteral("REQ-SW-PL-107"),
                                       QStringLiteral("ПРИЕТО"))}},
                      {{QStringLiteral("plugins/later.h"), QStringLiteral("// code\n")}}),
             "git fixture");

    Requirement req = parsedFixture(root, QStringLiteral("REQ-SW-PL-107"));
    QVERIFY2(evaluateDoneGate(req, {RequirementRoot{root}}),
             "the develop check must answer");

    QVERIFY2(!req.codeInDevelop, "the path is only on feat/x");
    QCOMPARE(req.missingDevelopPaths, QStringList{QStringLiteral("plugins/later.h")});
    QCOMPARE(req.codeBranch, QStringLiteral("feat/x"));
    QVERIFY(!req.doneCandidate);
    const QStringList missing = DoneGate::missingPreconditions(req);
    QCOMPARE(missing.size(), 1);
    QVERIFY2(missing.first().contains(QStringLiteral("още не е в")) && missing.first().contains(QStringLiteral("feat/x")),
             qPrintable(missing.first()));
}

void TestValidator::doneGate_refusesWhenNoDevelopBranch()
{
    // R1-1: a repo WITHOUT `develop` used to leave codeInDevelop == true — DONE
    // was granted without a single check. "Cannot check" is a refusal now, and
    // it says why.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString root = temp.path();
    QVERIFY2(makeRepo(root,
                      {{QString::fromUtf8(kReqPath),
                        doneReadyMarkdown(QStringLiteral("REQ-SW-PL-107"),
                                          QStringLiteral("IMPLEMENTED"),
                                          QStringLiteral("x"),
                                          QStringLiteral("plugins/later.h"))},
                       {QStringLiteral("plugins/later.h"), QStringLiteral("// code\n")},
                       {QString::fromUtf8(kReviewPath),
                        reviewMarkdown(QStringLiteral("REQ-SW-PL-107"),
                                       QStringLiteral("ПРИЕТО"))}},
                      {}, QStringLiteral("main")),
             "git fixture without develop");

    Requirement req = parsedFixture(root, QStringLiteral("REQ-SW-PL-107"));
    QVERIFY(!req.id.isEmpty());
    QVERIFY(req.allCriteriaDone);

    QVERIFY2(evaluateDoneGate(req, {RequirementRoot{root}}),
             "the develop check must answer");

    QVERIFY(req.acceptingReview);   // everything else holds: only the check is impossible
    QVERIFY2(!req.codeInDevelop, "no develop: the check cannot pass, so it refuses");
    QVERIFY2(req.codeCheckError.contains(QStringLiteral("develop")),
             qPrintable(req.codeCheckError));
    QVERIFY(req.missingDevelopPaths.isEmpty());   // nothing was found missing: nothing was asked
    QVERIFY(!req.doneCandidate);
    const QStringList missing = DoneGate::missingPreconditions(req);
    QCOMPARE(missing.size(), 1);
    QVERIFY2(missing.first().startsWith(
                 QStringLiteral("не мога да проверя дали кодът е в develop: ")),
             qPrintable(missing.first()));
    QVERIFY2(missing.first().contains(root), qPrintable(missing.first()));
}

void TestValidator::doneGate_publicPathResolvedByPublicDevelop()
{
    // R1-2: the tree spans two repos. A public path (`src/...`) does not exist in
    // the private repo's develop, and that is not a debt: any loaded repo's
    // develop carrying the path is an answer.
    QTemporaryDir privateDir;
    QTemporaryDir publicDir;
    QVERIFY(privateDir.isValid());
    QVERIFY(publicDir.isValid());
    const QString privateRoot = privateDir.path();
    const QString publicRoot = publicDir.path();
    QVERIFY2(makeRepo(privateRoot,
                      {{QString::fromUtf8(kReqPath),
                        doneReadyMarkdown(QStringLiteral("REQ-SW-PL-107"),
                                          QStringLiteral("IMPLEMENTED"),
                                          QStringLiteral("x"),
                                          QStringLiteral("src/frame_work/base/"
                                                         "PluginDiscovery.cpp"))},
                       {QString::fromUtf8(kReviewPath),
                        reviewMarkdown(QStringLiteral("REQ-SW-PL-107"),
                                       QStringLiteral("ПРИЕТО"))}},
                      {}),
             "private fixture");
    QVERIFY2(makeRepo(publicRoot,
                      {{QStringLiteral("src/frame_work/base/PluginDiscovery.cpp"),
                        QStringLiteral("// public\n")}},
                      {}),
             "public fixture");

    Requirement req = parsedFixture(privateRoot, QStringLiteral("REQ-SW-PL-107"));
    QVERIFY2(evaluateDoneGate(req, {RequirementRoot{privateRoot}, RequirementRoot{publicRoot}}),
             "the develop check must answer");

    QVERIFY2(req.codeInDevelop, "the public repo's develop carries the path");
    QVERIFY(req.codeCheckError.isEmpty());
    QVERIFY(req.missingDevelopPaths.isEmpty());
    QVERIFY(req.doneCandidate);
}

void TestValidator::doneGate_doneOutsideArchiveReported()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString root = temp.path();
    QVERIFY2(makeRepo(root,
                      {{QString::fromUtf8(kReqPath),
                        doneReadyMarkdown(QStringLiteral("REQ-SW-PL-107"),
                                          QStringLiteral("DONE"),
                                          QStringLiteral("x"),
                                          QStringLiteral("plugins/later.h"))},
                       {QStringLiteral("plugins/later.h"), QStringLiteral("// code\n")},
                       {QString::fromUtf8(kReviewPath),
                        reviewMarkdown(QStringLiteral("REQ-SW-PL-107"),
                                       QStringLiteral("ПРИЕТО"))}},
                      {}),
             "git fixture");

    Requirement req = parsedFixture(root, QStringLiteral("REQ-SW-PL-107"));
    QCOMPARE(req.section, QStringLiteral("active"));
    QVERIFY2(evaluateDoneGate(req, {RequirementRoot{root}}),
             "the develop check must answer");

    QVERIFY(!req.doneCandidate);   // only IMPLEMENTED is a candidate
    const QStringList missing = DoneGate::missingPreconditions(req);
    QCOMPARE(missing.size(), 1);
    QVERIFY2(missing.first().contains(QStringLiteral("archive")), qPrintable(missing.first()));
}

void TestValidator::revisionAboveApproved_error()
{
    // REQ-SW-PL-106: "одобрено" is a NUMBER, so a file that grew past it is a
    // fact the manager can report with both numbers — the same sentence
    // `scripts/check-req-revision.sh` prints.
    Requirement req = makeRequirement(QStringLiteral("REQ-SW-PL-910"),
                                      QStringLiteral("active"), QString());
    req.revision = 3;
    req.approvedRevision = 2;

    const QVector<RequirementsValidator::Issue> issues = RequirementsValidator::validate({req});
    int hits = 0;
    for (const RequirementsValidator::Issue &issue : issues) {
        if (issue.field != QStringLiteral("revision"))
            continue;
        ++hits;
        QCOMPARE(issue.severity, RequirementsValidator::Severity::Error);
        QVERIFY2(issue.message.contains(QStringLiteral("3")), qPrintable(issue.message));
        QVERIFY2(issue.message.contains(QStringLiteral("2")), qPrintable(issue.message));
    }
    QCOMPARE(hits, 1);

    // A DRAFT with the same numbers is not an implementation: its status is the
    // gate, and "revision 3, approved 2" is exactly what a contract change
    // looks like. Nothing extra is reported.
    Requirement draft = makeRequirement(QStringLiteral("REQ-SW-PL-911"),
                                       QStringLiteral("active"), QString(),
                                       QStringList(), QStringLiteral("DRAFT"));
    draft.revision = 3;
    draft.approvedRevision = 2;
    for (const RequirementsValidator::Issue &issue : RequirementsValidator::validate({draft})) {
        QVERIFY2(issue.field != QStringLiteral("revision"), qPrintable(issue.message));
    }
}

void TestValidator::revisionWithoutNumber_legacySilent()
{
    // 143 files carry "Одобрено: <дата>, потребител" with no number. The parser
    // normalises that to the file's own revision, and a file with no approval
    // line at all keeps -1 — neither may turn red for a format that did not
    // exist when they were written (REQ-SW-PL-106, „Нищо не се пренаписва назад“).
    Requirement normalised = makeRequirement(QStringLiteral("REQ-SW-PL-912"),
                                             QStringLiteral("active"), QString());
    normalised.revision = 4;
    normalised.approvedRevision = 4; // what the parser writes for a legacy line
    Requirement noLine = makeRequirement(QStringLiteral("REQ-SW-PL-913"),
                                        QStringLiteral("active"), QString());
    noLine.revision = 4;
    noLine.approvedRevision = -1;

    for (const RequirementsValidator::Issue &issue
         : RequirementsValidator::validate({normalised, noLine})) {
        QVERIFY2(issue.field != QStringLiteral("revision"), qPrintable(issue.message));
    }
}

// ── Връзка идея ↔ изискване (REQ-SW-PL-098) ────────────────────────────────
namespace {

Idea makeIdea(const QString &id, const QString &status, const QString &promoted)
{
    Idea idea;
    idea.id = id;
    idea.title = id;
    idea.status = status;
    idea.promotedId = promoted;
    idea.path = QStringLiteral("/tmp/%1.md").arg(id);
    return idea;
}

//!< A requirement whose body cites @p citedIds ("Изведено от IDEA-…").
Requirement requirementCiting(const QString &id, const QStringList &citedIds)
{
    Requirement req = makeRequirement(id, QStringLiteral("active"), QString());
    req.status = QStringLiteral("ACTIVE");
    QString body = QStringLiteral("# %1: Fixture\n\n## Защо\n\n").arg(id);
    for (const QString &cited : citedIds)
        body += QStringLiteral("- изведено от %1\n").arg(cited);
    req.rawContent = body;
    return req;
}

} // namespace

void TestValidator::ideaLink_citedIdeaWithoutRecord_error()
{
    // AC1: the requirement names the idea, the idea does not record the
    // promotion — the issue must name BOTH, so a reader does not have to guess
    // which of the two files to open.
    const Requirement req = requirementCiting(QStringLiteral("REQ-SW-PL-920"),
                                              {QStringLiteral("IDEA-003")});
    const Idea idea = makeIdea(QStringLiteral("IDEA-003"), QStringLiteral("нова"),
                              QString());

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validateIdeaLinks({req}, {idea});
    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).field, QStringLiteral("idea-link"));
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Error);
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("IDEA-003")),
             qPrintable(issues.at(0).message));
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("REQ-SW-PL-920")),
             qPrintable(issues.at(0).message));

    // The same citation with the promotion recorded is silent, and an idea that
    // nobody cites is not asked to record anything.
    const Idea promoted = makeIdea(QStringLiteral("IDEA-003"),
                                   QStringLiteral("промотирана"),
                                   QStringLiteral("REQ-SW-PL-920"));
    QCOMPARE(RequirementsValidator::validateIdeaLinks({req}, {promoted}).size(), 0);
}

void TestValidator::ideaLink_promotionToMissingRequirement_error()
{
    // AC2: "Изискване: REQ-X" names a file that does not exist. The message
    // carries the missing identifier — that is the repair.
    const Requirement req = requirementCiting(QStringLiteral("REQ-SW-PL-921"), {});
    const Idea idea = makeIdea(QStringLiteral("IDEA-004"), QStringLiteral("промотирана"),
                              QStringLiteral("REQ-SW-PL-999"));

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validateIdeaLinks({req}, {idea});
    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).field, QStringLiteral("idea-link"));
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Error);
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("REQ-SW-PL-999")),
             qPrintable(issues.at(0).message));
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("IDEA-004")),
             qPrintable(issues.at(0).message));
}

void TestValidator::ideaLink_promotedPairAndUnpromotedIdea_silent()
{
    // AC3: an idea without a requirement is the NORMAL state — neither an error
    // nor a warning. A correct pair produces nothing in either direction, and
    // the struck-through correction is not a citation.
    const Requirement req = requirementCiting(QStringLiteral("REQ-SW-PL-922"),
                                              {QStringLiteral("IDEA-002")});
    Requirement struck = req;
    struck.id = QStringLiteral("REQ-SW-PL-923");
    struck.rawContent += QStringLiteral("\n~~изведено от IDEA-005~~\n");

    QVector<Idea> ideas;
    ideas.append(makeIdea(QStringLiteral("IDEA-002"), QStringLiteral("промотирана"),
                          QStringLiteral("REQ-SW-PL-922")));
    ideas.append(makeIdea(QStringLiteral("IDEA-005"), QStringLiteral("нова"), QString()));
    ideas.append(makeIdea(QStringLiteral("IDEA-006"), QStringLiteral("паркирана"), QString()));

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validateIdeaLinks({req, struck}, ideas);
    QCOMPARE(issues.size(), 0);
}

void TestValidator::ideaLink_conflictingPromotionKeys_error()
{
    // Review R1-3 (REQ-SW-PL-098): the two spellings of one link disagree. The
    // reader keeps the first and would drop the second in silence, so the drift
    // ("the requirement was renamed") had no witness at all.
    const Requirement req = requirementCiting(QStringLiteral("REQ-SW-PL-924"), {});
    Idea idea = makeIdea(QStringLiteral("IDEA-009"), QStringLiteral("промотирана"),
                         QStringLiteral("REQ-SW-PL-924"));
    idea.promotedKeyId = QStringLiteral("REQ-SW-PL-924");
    idea.requirementKeyId = QStringLiteral("REQ-SW-PL-925");

    const QVector<RequirementsValidator::Issue> issues =
        RequirementsValidator::validateIdeaLinks({req}, {idea});
    QCOMPARE(issues.size(), 1);
    QCOMPARE(issues.at(0).field, QStringLiteral("idea-link"));
    QCOMPARE(issues.at(0).severity, RequirementsValidator::Severity::Error);
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("REQ-SW-PL-924")),
             qPrintable(issues.at(0).message));
    QVERIFY2(issues.at(0).message.contains(QStringLiteral("REQ-SW-PL-925")),
             qPrintable(issues.at(0).message));

    // The same link spelled twice is not a finding: only a disagreement is.
    idea.requirementKeyId = QStringLiteral("REQ-SW-PL-924");
    QCOMPARE(RequirementsValidator::validateIdeaLinks({req}, {idea}).size(), 0);
}
