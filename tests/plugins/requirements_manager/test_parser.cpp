#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QHash>
#include <QTextStream>

#include "RequirementsParser.h"
#include "RequirementsValidator.h"
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

// Bytes of a file, for "nothing was rewritten" assertions.
QString readAllText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(file.readAll());
}

// Full template with metadata, criteria and traceability.
QString fullMetadataContent()
{
    return QStringLiteral(
        "# REQ-SW-PL-001: Requirements Viewer/Editor Tool\n"
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
        "# REQ-SW-PL-002: Data Model Parsing Extensions\n"
        "\n"
        "- **Статус:** DONE\n"
        "- **Приоритет:** Medium\n"
        "- **Отговорник (роля):** Implementation\n"
        "- **Дата:** 2026-07-20\n"
        "- **Родител:** REQ-SW-PL-001\n"
        "- **Зависи от:** REQ-SW-PL-001, REQ-SW-PL-003\n"
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
        "# REQ-SW-PL-003: Minimal\n"
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

// The history block the parser appends when a status changes (REQ-SW-PL-101):
// the section does not exist yet, so it is created at the end of the file.
QString historyBlock(const QString &date, const QString &change, const QString &who,
                     const QString &why, const QString &eol = QString())
{
    const QString block =
        QStringLiteral("\n## История\n\n"
                       "| дата | промяна | кой | защо |\n"
                       "|---|---|---|---|\n"
                       "| %1 | %2 | %3 | %4 |\n")
            .arg(date, change, who, why);
    if (eol.isEmpty())
        return block;
    return QString(block).replace(QStringLiteral("\n"), eol);
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

// Minimal Requirement record for the phaseStatus() unit tests: only the three
// traceability fields that drive the phase flags are set.
Requirement phaseFixture(const QString &docs, const QString &code,
                         const QString &tests)
{
    Requirement req;
    req.id = QStringLiteral("REQ-SW-PL-017");
    req.title = QStringLiteral("Phase Status");
    req.docs = docs;
    req.code = code;
    req.tests = tests;
    return req;
}

} // namespace

void TestParser::parseDirectory_fullMetadata()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();

    const QString activeDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    const QString archiveDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/archive"));

    writeFixture(activeDir, QStringLiteral("REQ-SW-PL-001-requirements-viewer-editor-tool.md"),
                 fullMetadataContent());
    writeFixture(archiveDir, QStringLiteral("REQ-SW-PL-002-archived.md"), childMetadataContent());

    const QVector<Requirement> requirements = RequirementsParser::parseDirectory(baseDir);

    QCOMPARE(requirements.size(), 2);

    const Requirement *req1 = findRequirement(requirements, QStringLiteral("REQ-SW-PL-001"));
    QVERIFY2(req1, "REQ-SW-PL-001 should be parsed");
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
    QVERIFY(req1->filePath.endsWith(QStringLiteral("REQ-SW-PL-001-requirements-viewer-editor-tool.md")));
    QCOMPARE(req1->fileName, QStringLiteral("REQ-SW-PL-001-requirements-viewer-editor-tool.md"));

    const Requirement *req2 = findRequirement(requirements, QStringLiteral("REQ-SW-PL-002"));
    QVERIFY2(req2, "REQ-SW-PL-002 should be parsed");
    QCOMPARE(req2->parentId, QStringLiteral("REQ-SW-PL-001"));
    QCOMPARE(req2->dependencies.size(), 2);
    QCOMPARE(req2->dependencies.at(0), QStringLiteral("REQ-SW-PL-001"));
    QCOMPARE(req2->dependencies.at(1), QStringLiteral("REQ-SW-PL-003"));
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

    writeFixture(activeDir, QStringLiteral("REQ-SW-PL-003-minimal.md"), minimalContent());

    const QVector<Requirement> requirements = RequirementsParser::parseDirectory(baseDir);

    QCOMPARE(requirements.size(), 1);

    const Requirement *req = findRequirement(requirements, QStringLiteral("REQ-SW-PL-003"));
    QVERIFY2(req, "REQ-SW-PL-003 should be parsed");
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

void TestParser::parseDirectory_typedId()
{
    // Typed 4-segment scheme: "REQ-SW-<TYPE>-<NN>" (types FW/APP/PL/BLD)
    // must parse into id / title / section exactly like the 3-segment form.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();
    const QString activeDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));

    writeFixture(activeDir, QStringLiteral("REQ-SW-PL-001-some-title.md"),
                 requirementContent(QStringLiteral("REQ-SW-PL-001"),
                                    QStringLiteral("Some Title"), QStringLiteral("ACTIVE"),
                                    QStringLiteral("—"), QStringLiteral("—")));

    const QVector<Requirement> requirements = RequirementsParser::parseDirectory(baseDir);

    QCOMPARE(requirements.size(), 1);

    const Requirement *req = findRequirement(requirements, QStringLiteral("REQ-SW-PL-001"));
    QVERIFY2(req, "REQ-SW-PL-001 should be parsed");
    QCOMPARE(req->id, QStringLiteral("REQ-SW-PL-001"));
    QCOMPARE(req->title, QStringLiteral("Some Title"));
    QCOMPARE(req->section, QStringLiteral("active"));
}

void TestParser::parseDirectory_descriptionDialects()
{
    // The tree writes the description section three ways, one per template
    // generation. All three must produce a description; before the aliases a
    // requirement written with "## Накратко" or "## Проблем" looked
    // undescribed (60 warnings on the real tree).
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir = QDir(temp.path())
                                  .filePath(QStringLiteral("DevelopmentProcess/requirements/active"));

    const QString head = QStringLiteral("# REQ-SW-PL-%1: Dialect\n\n"
                                        "- **Статус:** ACTIVE\n\n");
    const QVector<QPair<QString, QString>> sections = {
        {QStringLiteral("001"), QStringLiteral("## Накратко")},
        {QStringLiteral("002"), QStringLiteral("## Проблем")},
        {QStringLiteral("003"), QStringLiteral("## Описание")},
    };
    for (const auto &entry : sections) {
        writeFixture(activeDir, QStringLiteral("%1.md").arg(entry.first),
                     head.arg(entry.first) + entry.second
                         + QStringLiteral("\n\nТекст на описанието.\n"));
    }

    const QVector<Requirement> requirements = RequirementsParser::parseDirectory(temp.path());

    QCOMPARE(requirements.size(), 3);
    for (const Requirement &req : requirements)
        QCOMPARE(req.description, QStringLiteral("Текст на описанието."));

    // The generation used is recorded (REQ-SW-PL-008): the validator grades a
    // deviation from it, and it must come from the heading, not from the file
    // name or the ID.
    QCOMPARE(findRequirement(requirements, QStringLiteral("REQ-SW-PL-001"))->templateVersion,
             QStringLiteral("v2"));
    QCOMPARE(findRequirement(requirements, QStringLiteral("REQ-SW-PL-002"))->templateVersion,
             QStringLiteral("v0"));
    QCOMPARE(findRequirement(requirements, QStringLiteral("REQ-SW-PL-003"))->templateVersion,
             QStringLiteral("v1"));

    // "## Цел" belongs to the same oldest generation as "## Проблем".
    writeFixture(activeDir, QStringLiteral("004.md"),
                 head.arg(QStringLiteral("004"))
                     + QStringLiteral("## Проблем\n\nПроблем.\n\n"
                                      "## Цел\n\nЦел.\n"));
    // No description heading at all: no template, and that is what the parser
    // must record — an empty version would be indistinguishable from a record
    // built by hand, which is never graded.
    writeFixture(activeDir, QStringLiteral("005.md"),
                 head.arg(QStringLiteral("005"))
                     + QStringLiteral("## Как ще разберем, че работи\n\n- [ ] x\n"));

    const QVector<Requirement> extended = RequirementsParser::parseDirectory(temp.path());
    QCOMPARE(extended.size(), 5);
    const Requirement *goal = findRequirement(extended, QStringLiteral("REQ-SW-PL-004"));
    QCOMPARE(goal->templateVersion, QStringLiteral("v0"));
    const Requirement *none = findRequirement(extended, QStringLiteral("REQ-SW-PL-005"));
    QCOMPARE(none->templateVersion, QStringLiteral("unknown"));
    QVERIFY(none->templateHeading.isEmpty());
}

void TestParser::parseDirectory_criteriaDialects()
{
    // Criteria arrive as checkbox bullets, as "### ACn" headings (with or
    // without a checkbox) and as a table inside the criteria section. Only the
    // bullet form was recognized, so 21 requirements looked criteria-less.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir = QDir(temp.path())
                                  .filePath(QStringLiteral("DevelopmentProcess/requirements/active"));

    writeFixture(activeDir, QStringLiteral("001.md"),
                 QStringLiteral(
                     "# REQ-SW-PL-001: Criteria Dialects\n\n"
                     "- **Статус:** ACTIVE\n\n"
                     "## Накратко\n\nОписание.\n\n"
                     "## Acceptance Criteria\n\n"
                     "### [x] AC1 — Заглавен критерий със отметка\n\n"
                     "### AC2 — Заглавен критерий без отметка\n\n"
                     "- [ ] 3. Точков критерий\n\n"
                     "1. Номериран критерий\n\n"
                     "| AC | Description | Status |\n"
                     "|---|---|---|\n"
                     "| 5 | Табличен критерий | ✅ |\n"
                     "| 6 | Табличен отворен | ☐ |\n"));
    // A numbered list and a table outside the criteria section are prose.
    writeFixture(activeDir, QStringLiteral("002.md"),
                 QStringLiteral(
                     "# REQ-SW-PL-002: Prose\n\n"
                     "- **Статус:** ACTIVE\n\n"
                     "## Накратко\n\n1. Не е критерий.\n\n"
                     "| 7 | Също не е критерий | ✅ |\n"));

    const QVector<Requirement> requirements = RequirementsParser::parseDirectory(temp.path());

    QCOMPARE(requirements.size(), 2);

    const Requirement *req = findRequirement(requirements, QStringLiteral("REQ-SW-PL-001"));
    QVERIFY2(req, "REQ-SW-PL-001 should be parsed");
    QCOMPARE(req->acceptanceCriteria.size(), 6);
    QCOMPARE(req->criteriaDone.size(), 6);
    QVERIFY(req->criteriaDone.at(0));
    QVERIFY(!req->criteriaDone.at(1)); // heading without checkbox is open
    QVERIFY(!req->criteriaDone.at(2));
    QVERIFY(!req->criteriaDone.at(3));
    QVERIFY(req->criteriaDone.at(4));
    QVERIFY(!req->criteriaDone.at(5));
    QCOMPARE(req->acceptanceCriteria.at(4), QStringLiteral("Табличен критерий"));

    const Requirement *prose = findRequirement(requirements, QStringLiteral("REQ-SW-PL-002"));
    QVERIFY2(prose, "REQ-SW-PL-002 should be parsed");
    QVERIFY(prose->acceptanceCriteria.isEmpty());
}

void TestParser::parseDirectory_criteriaHeadingBulgarian()
{
    // "## Критерии за приемане" is the fourth spelling of the criteria section
    // on the real tree (two files use it). It was not in the alias set, so its
    // "### ACn" headings were invisible: the requirement was parsed with zero
    // criteria and graded "no acceptance criteria defined" while the ACs were
    // plainly written. The checkbox bullet needs no heading (bullets are
    // recognized anywhere), which is exactly why the gap stayed hidden.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir = QDir(temp.path())
                                  .filePath(QStringLiteral("DevelopmentProcess/requirements/active"));

    writeFixture(activeDir, QStringLiteral("REQ-SW-PL-001-heading.md"),
                 QStringLiteral(
                     "# REQ-SW-PL-001: Критерии за приемане\n\n"
                     "- **Статус:** ACTIVE\n"
                     "- **Приоритет:** High\n"
                     "- **Дата:** 2026-11-16\n"
                     "- **Родител:** —\n"
                     "- **Зависи от:** —\n\n"
                     "## Накратко\n\nОписание.\n\n"
                     "## Критерии за приемане\n\n"
                     "### AC1 — Първи критерий\n\n"
                     "### AC2 — Втори критерий\n"));

    const QVector<Requirement> requirements = RequirementsParser::parseDirectory(temp.path());
    QCOMPARE(requirements.size(), 1);

    const Requirement *req = findRequirement(requirements, QStringLiteral("REQ-SW-PL-001"));
    QVERIFY2(req, "REQ-SW-PL-001 should be parsed");
    QCOMPARE(req->acceptanceCriteria.size(), 2);
    QCOMPARE(req->criteriaDone.size(), 2);
    QCOMPARE(req->acceptanceCriteria.at(0), QStringLiteral("Първи критерий"));
    QVERIFY(!req->criteriaDone.at(0)); // a heading without a checkbox stays open

    // What the gap actually cost: the validator's "no acceptance criteria"
    // warning. It must be gone for this file (other fields are filled, so the
    // criteria field is the only thing this case looks at).
    const QVector<RequirementsValidator::Issue> issues = RequirementsValidator::validate(requirements);
    for (const RequirementsValidator::Issue &issue : issues)
        QVERIFY2(issue.field != QStringLiteral("acceptanceCriteria"),
                 qPrintable(QStringLiteral("%1: %2").arg(issue.id, issue.message)));
}

void TestParser::parseDirectory_nestedSubdirSection()
{
    // A requirement nested below active/ (e.g. active/framework/) still
    // belongs to the "active" section - the section is derived from the
    // active|archive directory segment, not from the leaf subdirectory.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();
    const QString nestedDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active/framework"));

    writeFixture(nestedDir, QStringLiteral("REQ-SW-FW-001-some-framework.md"),
                 requirementContent(QStringLiteral("REQ-SW-FW-001"),
                                    QStringLiteral("Some Framework"), QStringLiteral("ACTIVE"),
                                    QStringLiteral("—"), QStringLiteral("—")));

    const QVector<Requirement> requirements = RequirementsParser::parseDirectory(baseDir);

    QCOMPARE(requirements.size(), 1);

    const Requirement *req = findRequirement(requirements, QStringLiteral("REQ-SW-FW-001"));
    QVERIFY2(req, "REQ-SW-FW-001 should be parsed");
    QCOMPARE(req->id, QStringLiteral("REQ-SW-FW-001"));
    QCOMPARE(req->section, QStringLiteral("active"));
}

void TestParser::parseReferences_annotationWithComma()
{
    // A reference is an ID token, not the text that happens to follow it. An
    // annotation may itself contain commas ("интероп слой — предшественик,
    // `DONE`"); splitting the value on every comma cut it in two and both
    // halves were then looked up as requirement IDs — 40 of the 46 errors the
    // GUI reported on the real tree while the CLI reported 2.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();
    const QString activeDir = QDir(baseDir).filePath(
        QStringLiteral("DevelopmentProcess/requirements/active"));

    writeFixture(activeDir, QStringLiteral("REQ-AI-009-registry.md"),
                 QStringLiteral(
                     "# REQ-AI-009: Registry\n"
                     "\n"
                     "- **Статус:** ACTIVE\n"
                     "- **Приоритет:** Medium\n"
                     "- **Отговорник (роля):** Implementation\n"
                     "- **Дата:** 2026-11-10\n"
                     "- **Родител:** —\n"
                     "- **Зависи от:** REQ-AI-005 (интероп слой — предшественик,"
                     " `DONE`), REQ-AI-010 (registry)\n"
                     "\n"
                     "## Описание\n"
                     "\n"
                     "Тест на референциите.\n"
                     "\n"
                     "## Acceptance Criteria\n"
                     "\n"
                     "- [ ] 1. Критерий.\n"));

    const QVector<Requirement> requirements = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(requirements.size(), 1);
    const Requirement &req = requirements.first();

    QCOMPARE(req.dependencies.size(), 2);
    QCOMPARE(req.dependencies.at(0), QStringLiteral("REQ-AI-005"));
    QCOMPARE(req.dependencies.at(1), QStringLiteral("REQ-AI-010"));
    QVERIFY(req.dependencyHints.value(QStringLiteral("REQ-AI-005"))
                .contains(QStringLiteral("предшественик")));
    QCOMPARE(req.dependencyHints.value(QStringLiteral("REQ-AI-010")),
             QStringLiteral("registry"));
}

void TestParser::parseReferences_multipleParents()
{
    // "Родител:" lists every parent; the tree is single-parent, so parentId is
    // the first requirement ID while parentIds keeps all of them resolvable.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();
    const QString activeDir = QDir(baseDir).filePath(
        QStringLiteral("DevelopmentProcess/requirements/active"));

    writeFixture(activeDir, QStringLiteral("REQ-AI-013-ladder.md"),
                 requirementContent(QStringLiteral("REQ-AI-013"),
                                    QStringLiteral("Ladder"),
                                    QStringLiteral("ACTIVE"),
                                    QStringLiteral("REQ-AI-011, REQ-AI-012"),
                                    QStringLiteral("—")));

    const QVector<Requirement> requirements = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(requirements.size(), 1);
    const Requirement &req = requirements.first();

    QCOMPARE(req.parentIds.size(), 2);
    QCOMPARE(req.parentIds.at(0), QStringLiteral("REQ-AI-011"));
    QCOMPARE(req.parentIds.at(1), QStringLiteral("REQ-AI-012"));
    QCOMPARE(req.parentId, QStringLiteral("REQ-AI-011"));
}

void TestParser::parseReferences_childrenAndNonRequirementId()
{
    // "Деца:" is the inverse of "Родител:" and is parsed the same way. A header
    // may also cite a document that is not a requirement (ADR-AI-013): the ID
    // token is kept so the validator can report it as "not a requirement"
    // instead of dropping it or looking it up as one.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();
    const QString activeDir = QDir(baseDir).filePath(
        QStringLiteral("DevelopmentProcess/requirements/active"));

    writeFixture(activeDir, QStringLiteral("REQ-AI-021-model-runtime.md"),
                 QStringLiteral(
                     "# REQ-AI-021: Model Runtime\n"
                     "\n"
                     "- **Статус:** ACTIVE\n"
                     "- **Приоритет:** High\n"
                     "- **Отговорник (роля):** Implementation\n"
                     "- **Дата:** 2026-11-01\n"
                     "- **Родител:** ADR-AI-013\n"
                     "- **Зависи от:** —\n"
                     "- **Деца:** REQ-AI-024, `REQ-AI-025`\n"
                     "\n"
                     "## Описание\n"
                     "\n"
                     "Тест на деца и не-REQ референция.\n"
                     "\n"
                     "## Acceptance Criteria\n"
                     "\n"
                     "- [ ] 1. Критерий.\n"));

    const QVector<Requirement> requirements = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(requirements.size(), 1);
    const Requirement &req = requirements.first();

    QCOMPARE(req.parentIds.size(), 1);
    QCOMPARE(req.parentIds.first(), QStringLiteral("ADR-AI-013"));
    QCOMPARE(req.parentId, QStringLiteral("ADR-AI-013"));
    QCOMPARE(req.childIds.size(), 2);
    QCOMPARE(req.childIds.at(0), QStringLiteral("REQ-AI-024"));
    QCOMPARE(req.childIds.at(1), QStringLiteral("REQ-AI-025"));
}

void TestParser::parseDirectory_traceabilityFields()
{
    // "- **Коммити:**" / "- **Код:**" / "- **Тестове:**" / "- **Документация:**"
    // lines must populate the structured fields AND still land in the raw
    // traceability text (fall-through, preserving the
    // "traceability.contains(commit)" contract).
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();
    const QString activeDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));

    writeFixture(activeDir, QStringLiteral("REQ-SW-PL-010-traceability-fields.md"),
                 QStringLiteral(
                     "# REQ-SW-PL-010: Traceability Fields\n"
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
                     "- **Тестове:** Qt5/Qt6 builds + unit tests\n"
                     "- **Документация:** docs/Architecture/plugins/README.md\n"));

    const QVector<Requirement> requirements = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(requirements.size(), 1);

    const Requirement *req = findRequirement(requirements, QStringLiteral("REQ-SW-PL-010"));
    QVERIFY2(req, "REQ-SW-PL-010 should be parsed");
    QCOMPARE(req->commits, QStringLiteral("abc123, def456"));
    QCOMPARE(req->code, QStringLiteral("src/plugins/requirements_manager/"));
    QCOMPARE(req->tests, QStringLiteral("Qt5/Qt6 builds + unit tests"));
    QCOMPARE(req->docs, QStringLiteral("docs/Architecture/plugins/README.md"));

    // The raw traceability text must still contain the same content.
    QVERIFY(req->traceability.contains(QStringLiteral("abc123")));
    QVERIFY(req->traceability.contains(QStringLiteral("def456")));
    QVERIFY(req->traceability.contains(QStringLiteral("src/plugins/requirements_manager/")));
    QVERIFY(req->traceability.contains(QStringLiteral("Qt5/Qt6 builds + unit tests")));
    QVERIFY(req->traceability.contains(QStringLiteral("docs/Architecture/plugins/README.md")));

    // A requirement without a Тестове line keeps the field empty.
    writeFixture(activeDir, QStringLiteral("REQ-SW-PL-011-no-tests.md"),
                 QStringLiteral(
                     "# REQ-SW-PL-011: No Tests\n"
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
    const Requirement *req11 = findRequirement(two, QStringLiteral("REQ-SW-PL-011"));
    QVERIFY2(req11, "REQ-SW-PL-011 should be parsed");
    QCOMPARE(req11->commits, QStringLiteral("deadbeef"));
    QVERIFY(req11->tests.isEmpty());
    QVERIFY(req11->traceability.contains(QStringLiteral("deadbeef")));

    // A "—" Документация value stays raw (no normalization) and reports the
    // architecture phase as not recorded.
    writeFixture(activeDir, QStringLiteral("REQ-SW-PL-012-dash-docs.md"),
                 QStringLiteral(
                     "# REQ-SW-PL-012: Dash Docs\n"
                     "\n"
                     "## Описание\n"
                     "\n"
                     "Без документация.\n"
                     "\n"
                     "## Проследимост\n"
                     "\n"
                     "- **Документация:** —\n"));
    const QVector<Requirement> three = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(three.size(), 3);
    const Requirement *req12 = findRequirement(three, QStringLiteral("REQ-SW-PL-012"));
    QVERIFY2(req12, "REQ-SW-PL-012 should be parsed");
    QCOMPARE(req12->docs, QStringLiteral("—"));
    QCOMPARE(phaseStatus(*req12).architecture, false);
}

void TestParser::phaseStatus_allEmpty_false()
{
    const PhaseStatus ps = phaseStatus(phaseFixture(QString(), QString(), QString()));

    QCOMPARE(ps.architecture, false);
    QCOMPARE(ps.implementation, false);
    QCOMPARE(ps.testing, false);
}

void TestParser::phaseStatus_dashAndWhitespace_false()
{
    // "—" (dash), whitespace-only and empty values are all "not recorded".
    const PhaseStatus ps = phaseStatus(phaseFixture(QStringLiteral("—"),
                                                    QStringLiteral("   "),
                                                    QString()));

    QCOMPARE(ps.architecture, false);
    QCOMPARE(ps.implementation, false);
    QCOMPARE(ps.testing, false);
}

void TestParser::phaseStatus_allFilled_true()
{
    const PhaseStatus ps = phaseStatus(phaseFixture(QStringLiteral("docs/"),
                                                    QStringLiteral("src/"),
                                                    QStringLiteral("tests/")));

    QCOMPARE(ps.architecture, true);
    QCOMPARE(ps.implementation, true);
    QCOMPARE(ps.testing, true);
}

void TestParser::phaseStatus_partialCode_only()
{
    // Only the code field is recorded -> only the implementation flag is set.
    const PhaseStatus ps = phaseStatus(phaseFixture(QString(),
                                                    QStringLiteral("src/"),
                                                    QString()));

    QCOMPARE(ps.architecture, false);
    QCOMPARE(ps.implementation, true);
    QCOMPARE(ps.testing, false);
}

void TestParser::phaseStatus_trimmedNonEmpty_true()
{
    // Surrounding whitespace is trimmed before the emptiness check.
    const PhaseStatus ps = phaseStatus(phaseFixture(QStringLiteral("  docs/  "),
                                                    QStringLiteral("src/"),
                                                    QStringLiteral("tests/")));

    QCOMPARE(ps.architecture, true);
    QCOMPARE(ps.implementation, true);
    QCOMPARE(ps.testing, true);
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

    writeFixture(activeDir, QStringLiteral("REQ-SW-PL-001.md"),
                 requirementContent(QStringLiteral("REQ-SW-PL-001"),
                                    QStringLiteral("One"), QStringLiteral("ACTIVE"),
                                    QStringLiteral("—"), QStringLiteral("—")));
    writeFixture(activeDir, QStringLiteral("REQ-SW-PL-002.md"),
                 requirementContent(QStringLiteral("REQ-SW-PL-002"),
                                    QStringLiteral("Two"), QStringLiteral("ACTIVE"),
                                    QStringLiteral("—"), QStringLiteral("—")));
    writeFixture(activeDir, QStringLiteral("REQ-SW-PL-003.md"),
                 requirementContent(QStringLiteral("REQ-SW-PL-003"),
                                    QStringLiteral("Three"), QStringLiteral("ACTIVE"),
                                    QStringLiteral("—"), QStringLiteral("—")));
    writeFixture(activeDir, QStringLiteral("REQ-PLG-005.md"),
                 requirementContent(QStringLiteral("REQ-PLG-005"),
                                    QStringLiteral("Plugin Five"), QStringLiteral("ACTIVE"),
                                    QStringLiteral("—"), QStringLiteral("—")));

    QCOMPARE(RequirementsParser::generateNextId(baseDir, QStringLiteral("SW-PL")),
             QStringLiteral("REQ-SW-PL-004"));
    QCOMPARE(RequirementsParser::generateNextId(baseDir, QStringLiteral("PLG")),
             QStringLiteral("REQ-PLG-006"));

    // A requirement in archive/ must also be considered (active AND archive).
    writeFixture(archiveDir, QStringLiteral("REQ-SW-PL-010-archived.md"),
                 requirementContent(QStringLiteral("REQ-SW-PL-010"),
                                    QStringLiteral("Ten"), QStringLiteral("DONE"),
                                    QStringLiteral("—"), QStringLiteral("—")));
    QCOMPARE(RequirementsParser::generateNextId(baseDir, QStringLiteral("SW-PL")),
             QStringLiteral("REQ-SW-PL-011"));
}

void TestParser::generateNextId_typedIsolation()
{
    // Typed families must be isolated: the max+1 scan for "SW-PL" may only
    // count REQ-SW-PL-* and must ignore REQ-SW-FW-*/REQ-SW-APP-*/REQ-SW-BLD-*.
    // 3-segment private families (REQ-PLG-*) stay fully separate as well.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();
    const QString activeDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));

    const auto write = [&](const QString &id) {
        writeFixture(activeDir, id + QStringLiteral(".md"),
                     requirementContent(id, id, QStringLiteral("ACTIVE"),
                                        QStringLiteral("—"), QStringLiteral("—")));
    };

    write(QStringLiteral("REQ-SW-PL-001"));
    write(QStringLiteral("REQ-SW-PL-005"));
    write(QStringLiteral("REQ-SW-FW-003"));
    write(QStringLiteral("REQ-SW-APP-001"));
    write(QStringLiteral("REQ-SW-BLD-002"));
    // 3-segment compatibility family (private prefix).
    write(QStringLiteral("REQ-PLG-001"));
    write(QStringLiteral("REQ-PLG-002"));

    QCOMPARE(RequirementsParser::generateNextId(baseDir, QStringLiteral("SW-PL")),
             QStringLiteral("REQ-SW-PL-006"));
    QCOMPARE(RequirementsParser::generateNextId(baseDir, QStringLiteral("SW-FW")),
             QStringLiteral("REQ-SW-FW-004"));
    QCOMPARE(RequirementsParser::generateNextId(baseDir, QStringLiteral("SW-APP")),
             QStringLiteral("REQ-SW-APP-002"));
    QCOMPARE(RequirementsParser::generateNextId(baseDir, QStringLiteral("SW-BLD")),
             QStringLiteral("REQ-SW-BLD-003"));
    // 3-segment private family: still works and is not polluted by the typed
    // REQ-SW-* families (and vice versa).
    QCOMPARE(RequirementsParser::generateNextId(baseDir, QStringLiteral("PLG")),
             QStringLiteral("REQ-PLG-003"));
}

void TestParser::moveToArchive_movesFile()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();
    const QString activeDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));

    writeFixture(activeDir, QStringLiteral("REQ-SW-PL-001.md"),
                 requirementContent(QStringLiteral("REQ-SW-PL-001"),
                                    QStringLiteral("One"), QStringLiteral("ACTIVE"),
                                    QStringLiteral("—"), QStringLiteral("—")));

    const QVector<Requirement> before = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(before.size(), 1);
    QCOMPARE(before.first().section, QStringLiteral("active"));

    QVERIFY(RequirementsParser::moveToArchive(before.first().filePath));

    QVERIFY(!QFile::exists(before.first().filePath));
    const QString archiveDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/archive"));
    QVERIFY(QFile::exists(QDir(archiveDir).filePath(QStringLiteral("REQ-SW-PL-001.md"))));

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

    writeFixture(archiveDir, QStringLiteral("REQ-SW-PL-001.md"),
                 requirementContent(QStringLiteral("REQ-SW-PL-001"),
                                    QStringLiteral("One"), QStringLiteral("DONE"),
                                    QStringLiteral("—"), QStringLiteral("—")));

    const QVector<Requirement> before = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(before.size(), 1);
    QCOMPARE(before.first().section, QStringLiteral("archive"));

    QVERIFY(RequirementsParser::moveToActive(before.first().filePath));

    QVERIFY(!QFile::exists(before.first().filePath));
    const QString activeDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    QVERIFY(QFile::exists(QDir(activeDir).filePath(QStringLiteral("REQ-SW-PL-001.md"))));

    const QVector<Requirement> after = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(after.size(), 1);
    QCOMPARE(after.first().section, QStringLiteral("active"));
}

void TestParser::moveToArchive_neverOverwrites()
{
    // REQ-SW-PL-107 AC4: the move is strict. An archive file of the same name is
    // an error and its bytes stay untouched — "нищо не се трие".
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();
    const QString activeDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    const QString archiveDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/archive"));

    const QString activePath = writeFixture(
        activeDir, QStringLiteral("REQ-SW-PL-001.md"),
        requirementContent(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("One"),
                           QStringLiteral("ACTIVE"), QStringLiteral("—"), QStringLiteral("—")));
    const QString archivedPath = writeFixture(
        archiveDir, QStringLiteral("REQ-SW-PL-001.md"),
        requirementContent(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("One"),
                           QStringLiteral("DONE"), QStringLiteral("—"), QStringLiteral("—")));
    const QString archivedBefore = readAllText(archivedPath);

    QVERIFY2(!RequirementsParser::moveToArchive(activePath),
             "an existing archive file must refuse the move");
    QVERIFY2(QFile::exists(activePath), "the active file stays where it was");
    QCOMPARE(readAllText(archivedPath), archivedBefore);   // not overwritten
}

void TestParser::markDone_rollsBackWhenArchiveTargetExists()
{
    // R1-5: the status write and the move are ONE change. When the move fails
    // (an archive file of the same name is there — moveToArchive never
    // overwrites), the DONE line must not survive in active/.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();
    const QString activeDir =
        QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    const QString archiveDir =
        QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/archive"));

    const QString activePath = writeFixture(
        activeDir, QStringLiteral("REQ-SW-PL-001.md"),
        requirementContent(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("One"),
                           QStringLiteral("IMPLEMENTED"), QStringLiteral("—"),
                           QStringLiteral("—")));
    const QString archivedPath = writeFixture(
        archiveDir, QStringLiteral("REQ-SW-PL-001.md"),
        requirementContent(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("One"),
                           QStringLiteral("DONE"), QStringLiteral("—"),
                           QStringLiteral("—")));
    const QString activeBefore = readAllText(activePath);
    const QString archivedBefore = readAllText(archivedPath);

    QString error;
    QVERIFY2(!RequirementsParser::markDone(
                 activePath, QStringLiteral("plans/reviews/PR-1-REQ-SW-PL-001.md"),
                 QStringLiteral("2026-11-22"), &error),
             "the move cannot happen, so the verdict cannot be recorded");
    QVERIFY2(error.contains(QStringLiteral("archive")), qPrintable(error));

    QCOMPARE(readAllText(activePath), activeBefore);   // byte for byte: no DONE left behind
    QCOMPARE(readAllText(archivedPath), archivedBefore);
}

void TestParser::moveToArchive_nestedSubdir(){    // Archiving a requirement that lives below active/<subdir>/ must preserve
    // the subdirectory: active/framework/ -> archive/framework/. The section
    // follows the move in both directions.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();
    const QString nestedActive = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active/framework"));

    writeFixture(nestedActive, QStringLiteral("REQ-SW-FW-001-some-framework.md"),
                 requirementContent(QStringLiteral("REQ-SW-FW-001"),
                                    QStringLiteral("Some Framework"), QStringLiteral("ACTIVE"),
                                    QStringLiteral("—"), QStringLiteral("—")));

    const QVector<Requirement> before = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(before.size(), 1);
    QCOMPARE(before.first().section, QStringLiteral("active"));
    QVERIFY(before.first().filePath.contains(QStringLiteral("/active/framework/")));

    QVERIFY(RequirementsParser::moveToArchive(before.first().filePath));
    QVERIFY(!QFile::exists(before.first().filePath));
    const QString archivedPath = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/archive/framework/REQ-SW-FW-001-some-framework.md"));
    QVERIFY(QFile::exists(archivedPath));

    const QVector<Requirement> archived = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(archived.size(), 1);
    QCOMPARE(archived.first().section, QStringLiteral("archive"));
    QVERIFY(archived.first().filePath.contains(QStringLiteral("/archive/framework/")));

    // Reopen: archive/framework/ -> active/framework/, section becomes active.
    QVERIFY(RequirementsParser::moveToActive(archived.first().filePath));
    const QString reopenedPath = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active/framework/REQ-SW-FW-001-some-framework.md"));
    QVERIFY(QFile::exists(reopenedPath));
    QVERIFY(!QFile::exists(archivedPath));

    const QVector<Requirement> reopened = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(reopened.size(), 1);
    QCOMPARE(reopened.first().section, QStringLiteral("active"));
    QVERIFY(reopened.first().filePath.contains(QStringLiteral("/active/framework/")));
}

void TestParser::moveRejectsWrongSection()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString baseDir = temp.path();
    const QString activeDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    const QString archiveDir = QDir(baseDir).filePath(QStringLiteral("DevelopmentProcess/requirements/archive"));

    writeFixture(activeDir, QStringLiteral("REQ-SW-PL-001.md"),
                 requirementContent(QStringLiteral("REQ-SW-PL-001"),
                                    QStringLiteral("One"), QStringLiteral("ACTIVE"),
                                    QStringLiteral("—"), QStringLiteral("—")));
    writeFixture(archiveDir, QStringLiteral("REQ-SW-PL-002.md"),
                 requirementContent(QStringLiteral("REQ-SW-PL-002"),
                                    QStringLiteral("Two"), QStringLiteral("DONE"),
                                    QStringLiteral("—"), QStringLiteral("—")));
    writeFixture(temp.path(), QStringLiteral("loose.md"),
                 requirementContent(QStringLiteral("REQ-SW-PL-003"),
                                    QStringLiteral("Three"), QStringLiteral("ACTIVE"),
                                    QStringLiteral("—"), QStringLiteral("—")));

    const QVector<Requirement> requirements = RequirementsParser::parseDirectory(baseDir);

    // Moving an active file to archive is valid, moving an archive file to
    // archive again is not.
    const Requirement *req1 = findRequirement(requirements, QStringLiteral("REQ-SW-PL-001"));
    const Requirement *req2 = findRequirement(requirements, QStringLiteral("REQ-SW-PL-002"));
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

    writeFixture(activeDir, QStringLiteral("REQ-SW-PL-001.md"),
                 requirementContent(QStringLiteral("REQ-SW-PL-001"),
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
    RequirementsParser::setDependenciesLine(req, {QStringLiteral("REQ-SW-PL-002"),
                                                  QStringLiteral("REQ-SW-PL-003")});
    QVERIFY(RequirementsParser::writeRequirement(req));

    reparsed = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(reparsed.size(), 1);
    QCOMPARE(reparsed.first().dependencies.size(), 2);
    QCOMPARE(reparsed.first().dependencies.at(0), QStringLiteral("REQ-SW-PL-002"));
    QCOMPARE(reparsed.first().dependencies.at(1), QStringLiteral("REQ-SW-PL-003"));

    // 4. Clearing the dependency list writes "—".
    req = reparsed.first();
    RequirementsParser::setDependenciesLine(req, {});
    QVERIFY(RequirementsParser::writeRequirement(req));

    reparsed = RequirementsParser::parseDirectory(baseDir);
    QCOMPARE(reparsed.size(), 1);
    QVERIFY(reparsed.first().dependencies.isEmpty());
    QVERIFY(reparsed.first().rawContent.contains(QStringLiteral("- **Зависи от:** —")));
}

void TestParser::approveDraft_rewritesStatusAndApproval()
{
    // REQ-SW-PL-099: the Manager's „Приеми“ is the only path DRAFT -> ACTIVE.
    // It rewrites exactly two lines and leaves every other byte of the file as
    // it was (the same promise the idea promotion makes).
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir =
        QDir(temp.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    const QString before =
        QString(requirementContent(QStringLiteral("REQ-SW-PL-096"),
                                   QStringLiteral("Draft"), QStringLiteral("DRAFT"),
                                   QStringLiteral("—"), QStringLiteral("—")))
            // The banner a draft really carries (REQ-SW-PL-099): approval must
            // drop it, or the ACTIVE file still says it waits for approval.
            .replace(QStringLiteral("- **Зависи от:** —\n\n"),
                     QStringLiteral("- **Зависи от:** —\n\n"
                                    "> **Чернова — чака одобрение в мениджъра; не се "
                                    "имплементира, докато не стане `ACTIVE`.**\n\n"));
    const QString path = writeFixture(activeDir, QStringLiteral("DRAFT-REQ-SW-PL-096-draft.md"),
                                      before);
    QVERIFY(!path.isEmpty());

    QString error;
    QVERIFY2(RequirementsParser::approveDraft(path, QStringLiteral("2026-11-21"), &error),
             qPrintable(error));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString after = QString::fromUtf8(file.readAll());
    file.close();

    // Byte-exact: the banner line and the blank line it would leave behind are
    // gone, the two approval lines replace the status, and the history row the
    // status change demands (REQ-SW-PL-101) is the only addition at the end.
    // The approval seals the CONTRACT too (REQ-SW-PL-109): the revision stays 1
    // because a status change and a history row are bookkeeping, not a contract.
    const QString contract = RequirementsParser::contractHash(before);
    QVERIFY2(!contract.isEmpty(), "the fixture carries a title and one AC");
    const QString expected =
        QString(before)
            .remove(QStringLiteral("> **Чернова — чака одобрение в мениджъра; не се "
                                   "имплементира, докато не стане `ACTIVE`.**\n\n"))
            .replace(QStringLiteral("- **Статус:** DRAFT"),
                     QStringLiteral("- **Статус:** ACTIVE\n"
                                    "- **Одобрено:** 2026-11-21, потребител "
                                    "(ревизия 1, договор %1)\n"
                                    "- **Ревизия:** 1")
                         .arg(contract))
        + historyBlock(QStringLiteral("2026-11-21"),
                       QStringLiteral("одобрено (DRAFT → ACTIVE)"),
                       QStringLiteral("потребител"),
                       QStringLiteral("присъда на потребителя"));
    QCOMPARE(after, expected);
    QVERIFY(after.contains(QStringLiteral("- **Одобрено:** 2026-11-21, потребител "
                                          "(ревизия 1, договор %1)")
                               .arg(contract)));
    QVERIFY2(!after.contains(QStringLiteral("Чернова")),
             "an accepted file must not claim it is still a draft");

    // The parser reads the new status, so the tree shows ACTIVE at once.
    const QVector<Requirement> parsed = RequirementsParser::parseDirectory(temp.path());
    QCOMPARE(parsed.size(), 1);
    QCOMPARE(parsed.first().status, QStringLiteral("ACTIVE"));
    // …and the revision that grew with the row is visible in the manager.
    QCOMPARE(parsed.first().revision, 1);
}

void TestParser::approveDraft_secondAcceptIsNoOp()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir =
        QDir(temp.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    const QString path = writeFixture(
        activeDir, QStringLiteral("DRAFT-REQ-SW-PL-097-draft.md"),
        requirementContent(QStringLiteral("REQ-SW-PL-097"), QStringLiteral("Draft"),
                           QStringLiteral("DRAFT"), QStringLiteral("—"),
                           QStringLiteral("—")));
    QVERIFY(!path.isEmpty());
    QVERIFY(RequirementsParser::approveDraft(path, QStringLiteral("2026-11-21")));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString first = QString::fromUtf8(file.readAll());
    file.close();

    // Already ACTIVE: a second accept must not re-date it, nor write at all.
    QVERIFY(RequirementsParser::approveDraft(path, QStringLiteral("2026-12-01")));

    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString second = QString::fromUtf8(file.readAll());
    file.close();
    QCOMPARE(second, first);
    QVERIFY(second.contains(QStringLiteral("2026-11-21")));
    QVERIFY(!second.contains(QStringLiteral("2026-12-01")));
}

void TestParser::approveDraft_nonDraftNoOp()
{
    // An ACTIVE requirement is not a draft: the call succeeds and writes
    // nothing, so the file keeps its bytes exactly.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir =
        QDir(temp.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    const QString before =
        requirementContent(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("Active"),
                           QStringLiteral("ACTIVE"), QStringLiteral("—"),
                           QStringLiteral("—"));
    const QString path = writeFixture(activeDir, QStringLiteral("REQ-SW-PL-001.md"), before);
    QVERIFY(!path.isEmpty());

    QVERIFY(RequirementsParser::approveDraft(path, QStringLiteral("2026-11-21")));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(file.readAll()), before);
}

void TestParser::approveDraft_missingStatus_error()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = writeFixture(
        temp.path(), QStringLiteral("DRAFT-REQ-SW-PL-098-draft.md"),
        QStringLiteral("# REQ-SW-PL-098: Няма статус\n\n## Накратко\n\nТяло.\n"));
    QVERIFY(!path.isEmpty());

    QString error;
    QVERIFY(!RequirementsParser::approveDraft(path, QStringLiteral("2026-11-21"), &error));
    QVERIFY2(!error.isEmpty(), "a failed approval must say why");
    QVERIFY(error.contains(QStringLiteral("Статус")));
}

void TestParser::approveDraft_keepsCrlfBytes(){
    // A CRLF file stays CRLF: the inserted line carries the same ending and no
    // other byte changes. The fixture is written in binary mode — the point is
    // the bytes on disk, not a text-mode round-trip.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString before =
        QStringLiteral("# REQ-SW-PL-099: CRLF\r\n"
                       "\r\n"
                       "- **Статус:** DRAFT\r\n"
                       "- **Приоритет:** Low\r\n");
    const QString path = QDir(temp.path()).filePath(
        QStringLiteral("DRAFT-REQ-SW-PL-099-draft.md"));
    {
        QFile fixture(path);
        QVERIFY(fixture.open(QIODevice::WriteOnly));
        fixture.write(before.toUtf8());
    }

    QVERIFY(RequirementsParser::approveDraft(path, QStringLiteral("2026-11-21")));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString after = QString::fromUtf8(file.readAll());
    const QString contract = RequirementsParser::contractHash(before);
    QVERIFY2(!contract.isEmpty(), "the fixture carries a title");
    const QString expected =
        QString(before)
            .replace(QStringLiteral("- **Статус:** DRAFT\r\n"),
                     QStringLiteral("- **Статус:** ACTIVE\r\n"
                                    "- **Одобрено:** 2026-11-21, потребител "
                                    "(ревизия 1, договор %1)\r\n"
                                    "- **Ревизия:** 1\r\n")
                         .arg(contract))
        + historyBlock(QStringLiteral("2026-11-21"),
                       QStringLiteral("одобрено (DRAFT → ACTIVE)"),
                       QStringLiteral("потребител"),
                       QStringLiteral("присъда на потребителя"),
                       QStringLiteral("\r\n"));
    QCOMPARE(after, expected);
}

// ── REQ-SW-PL-101: created date, revision, contract change ─────────────────
void TestParser::parseDirectory_createdAndRevision()
{
    // A file written with the new template carries "Създадено:" and "Ревизия:".
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir =
        QDir(temp.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    const QString path = writeFixture(
        activeDir, QStringLiteral("REQ-SW-PL-901.md"),
        QStringLiteral("# REQ-SW-PL-901: Нов шаблон\n"
                       "\n"
                       "- **Статус:** ACTIVE\n"
                       "- **Създадено:** 2026-11-21\n"
                       "- **Ревизия:** 3\n"
                       "- **Дата:** 2026-01-02\n"
                       "\n"
                       "## Накратко\n\nТяло.\n"));
    QVERIFY(!path.isEmpty());

    const QVector<Requirement> parsed = RequirementsParser::parseDirectory(temp.path());
    QCOMPARE(parsed.size(), 1);
    QCOMPARE(parsed.first().revision, 3);
    // "Създадено:" wins over the legacy field when both are present.
    QCOMPARE(parsed.first().created, QStringLiteral("2026-11-21"));
    QCOMPARE(createdDate(parsed.first()), QStringLiteral("2026-11-21"));
}

void TestParser::parseDirectory_legacyDateIsCreationAlias()
{
    // AC2/AC3: the 135 files written before "Създадено:" keep their bytes and
    // still answer "when was this created" — the alias lives in the parser.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir =
        QDir(temp.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    QVERIFY(!writeFixture(activeDir, QStringLiteral("REQ-SW-PL-902.md"),
                          requirementContent(QStringLiteral("REQ-SW-PL-902"),
                                             QStringLiteral("Стар файл"),
                                             QStringLiteral("ACTIVE"),
                                             QStringLiteral("—"), QStringLiteral("—")))
                 .isEmpty());

    const QVector<Requirement> parsed = RequirementsParser::parseDirectory(temp.path());
    QCOMPARE(parsed.size(), 1);
    QCOMPARE(parsed.first().date, QStringLiteral("2026-07-01"));
    QCOMPARE(parsed.first().created, QStringLiteral("2026-07-01"));
    QCOMPARE(parsed.first().revision, 0); // no field: not a zero revision, an absent one

    // A record built by hand (a test fixture, a caller) behaves the same.
    Requirement bare;
    bare.date = QStringLiteral("2026-10-03");
    QCOMPARE(createdDate(bare), QStringLiteral("2026-10-03"));
    bare.created = QStringLiteral("2026-10-01");
    QCOMPARE(createdDate(bare), QStringLiteral("2026-10-01"));
}

void TestParser::requestChangesDraft_appendsOneRowOnly()
{
    // REQ-SW-PL-102 AC1: status stays DRAFT, ONE row appears, nothing else.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir =
        QDir(temp.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    const QString before =
        requirementContent(QStringLiteral("REQ-SW-PL-903"), QStringLiteral("Чернова"),
                           QStringLiteral("DRAFT"), QStringLiteral("—"),
                           QStringLiteral("—"));
    const QString path = writeFixture(activeDir, QStringLiteral("REQ-SW-PL-903.md"), before);
    QVERIFY(!path.isEmpty());

    QString error;
    QVERIFY2(RequirementsParser::requestChangesDraft(
                 path, QStringLiteral("липсва AC за Qt5"), QStringLiteral("2026-11-21"),
                 &error),
             qPrintable(error));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString after = QString::fromUtf8(file.readAll());
    file.close();

    const QString expected =
        QString(before)
        + QStringLiteral("\n## Ревю на черновата\n\n"
                         "| кръг | дата | присъда | бележка |\n"
                         "|---|---|---|---|\n"
                         "| 1 | 2026-11-21 | за корекция | липсва AC за Qt5 |\n");
    QCOMPARE(after, expected);
    QVERIFY(after.contains(QStringLiteral("- **Статус:** DRAFT")));
    QCOMPARE(RequirementsParser::lastDraftVerdict(after),
             RequirementsParser::changesRequestedVerdict());
    QCOMPARE(RequirementsParser::draftReviewRounds(after), 1);

    // A second round is numbered 2 and lands under the same table.
    QVERIFY(RequirementsParser::requestChangesDraft(
        path, QStringLiteral("още липсва"), QStringLiteral("2026-11-22")));
    QFile again(path);
    QVERIFY(again.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString twice = QString::fromUtf8(again.readAll());
    again.close();
    QVERIFY2(twice.contains(QStringLiteral("| 2 | 2026-11-22 | за корекция | още липсва |")),
             qPrintable(twice));
    QCOMPARE(RequirementsParser::draftReviewRounds(twice), 2);
    QCOMPARE(twice.count(QStringLiteral("## Ревю на черновата")), 1);
}

void TestParser::requestChangesDraft_rejectsEmptyNoteAndNonDraft()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir =
        QDir(temp.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    const QString draft =
        writeFixture(activeDir, QStringLiteral("REQ-SW-PL-904.md"),
                     requirementContent(QStringLiteral("REQ-SW-PL-904"),
                                        QStringLiteral("Чернова"), QStringLiteral("DRAFT"),
                                        QStringLiteral("—"), QStringLiteral("—")));
    QVERIFY(!draft.isEmpty());

    QString error;
    QVERIFY(!RequirementsParser::requestChangesDraft(draft, QStringLiteral("   "),
                                                     QStringLiteral("2026-11-21"), &error));
    QVERIFY2(error.contains(QStringLiteral("бележката е празна")), qPrintable(error));

    // An ACTIVE file is not a draft: the call succeeds and writes nothing.
    const QString active =
        writeFixture(activeDir, QStringLiteral("REQ-SW-PL-905.md"),
                     requirementContent(QStringLiteral("REQ-SW-PL-905"),
                                        QStringLiteral("Активно"), QStringLiteral("ACTIVE"),
                                        QStringLiteral("—"), QStringLiteral("—")));
    QFile file(active);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString before = QString::fromUtf8(file.readAll());
    file.close();
    QVERIFY(RequirementsParser::requestChangesDraft(active, QStringLiteral("бележка"),
                                                    QStringLiteral("2026-11-21")));
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(file.readAll()), before);
}

void TestParser::rejectDraft_cancelsAndArchives()
{
    // REQ-SW-PL-102 AC2: CANCELLED + review row + history row, file in archive/.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir =
        QDir(temp.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    const QString path = writeFixture(
        activeDir, QStringLiteral("REQ-SW-PL-906.md"),
        requirementContent(QStringLiteral("REQ-SW-PL-906"), QStringLiteral("Отказ"),
                           QStringLiteral("DRAFT"), QStringLiteral("—"),
                           QStringLiteral("—")));
    QVERIFY(!path.isEmpty());

    QString error;
    QVERIFY2(RequirementsParser::rejectDraft(
                 path, QStringLiteral("дубликат на REQ-SW-PL-099"),
                 QStringLiteral("2026-11-21"), &error),
             qPrintable(error));

    QVERIFY2(!QFile::exists(path), "a CANCELLED file must not stay in active/");
    const QString archived = QDir(temp.path()).filePath(
        QStringLiteral("DevelopmentProcess/requirements/archive/REQ-SW-PL-906.md"));
    QVERIFY(QFile::exists(archived));

    QFile file(archived);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString after = QString::fromUtf8(file.readAll());
    file.close();
    QVERIFY(after.contains(QStringLiteral("- **Статус:** CANCELLED")));
    QVERIFY(after.contains(QStringLiteral("| 1 | 2026-11-21 | откажи | дубликат на "
                                          "REQ-SW-PL-099 |")));
    QVERIFY2(after.contains(QStringLiteral("| 2026-11-21 | CANCELLED")),
             "the status changed, so the history says why");

    // Empty reason is no verdict; and an existing file in archive/ is never
    // overwritten ("нищо не се трие").
    QTemporaryDir second;
    QVERIFY(second.isValid());
    const QString activeDir2 =
        QDir(second.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    const QString secondPath =
        writeFixture(activeDir2, QStringLiteral("REQ-SW-PL-907.md"),
                     requirementContent(QStringLiteral("REQ-SW-PL-907"),
                                        QStringLiteral("Отказ"), QStringLiteral("DRAFT"),
                                        QStringLiteral("—"), QStringLiteral("—")));
    QVERIFY(!secondPath.isEmpty());
    QVERIFY(!RequirementsParser::rejectDraft(secondPath, QStringLiteral(" "),
                                             QStringLiteral("2026-11-21"), &error));

    const QString archiveDir2 =
        QDir(second.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/archive"));
    QVERIFY(!writeFixture(archiveDir2, QStringLiteral("REQ-SW-PL-907.md"),
                          QStringLiteral("вече е тук\n")).isEmpty());
    error.clear();
    QVERIFY(!RequirementsParser::rejectDraft(secondPath, QStringLiteral("не става"),
                                             QStringLiteral("2026-11-21"), &error));
    QVERIFY2(error.contains(QStringLiteral("вече има файл")), qPrintable(error));

    QFile kept(QDir(archiveDir2).filePath(QStringLiteral("REQ-SW-PL-907.md")));
    QVERIFY(kept.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(kept.readAll()), QStringLiteral("вече е тук\n"));
}

void TestParser::startContractChange_backToDraftWithHistory()
{
    // REQ-SW-PL-101 AC4: a contract change flips to DRAFT and leaves a row and
    // a revision bump behind; a draft has no contract to change.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir =
        QDir(temp.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    const QString path = writeFixture(
        activeDir, QStringLiteral("REQ-SW-PL-908.md"),
        requirementContent(QStringLiteral("REQ-SW-PL-908"), QStringLiteral("Договор"),
                           QStringLiteral("ACTIVE"), QStringLiteral("—"),
                           QStringLiteral("—")));
    QVERIFY(!path.isEmpty());

    QString error;
    QVERIFY2(RequirementsParser::startContractChange(
                 path, QStringLiteral("добавя се AC7"), QStringLiteral("потребител"),
                 QStringLiteral("2026-11-22"), &error),
             qPrintable(error));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString after = QString::fromUtf8(file.readAll());
    file.close();
    QVERIFY(after.contains(QStringLiteral("- **Статус:** DRAFT")));
    QVERIFY(after.contains(QStringLiteral("- **Ревизия:** 1")));
    QVERIFY(after.contains(QStringLiteral("промяна в договора")));
    QVERIFY(after.contains(QStringLiteral("добавя се AC7")));

    // Now it is a draft: a second "change the contract" is a no-op.
    QFile same(path);
    QVERIFY(same.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString first = QString::fromUtf8(same.readAll());
    same.close();
    QVERIFY(RequirementsParser::startContractChange(
        path, QStringLiteral("пак"), QStringLiteral("потребител"),
        QStringLiteral("2026-11-23")));
    QVERIFY(same.open(QIODevice::ReadOnly | QIODevice::Text));
    QCOMPARE(QString::fromUtf8(same.readAll()), first);
}

void TestParser::parseDirectory_approvedRevisionAndBaseline()
{
    // REQ-SW-PL-106: "Одобрено:" names the revision it accepted, so "approved"
    // has a number instead of a date that cannot answer what was approved. The
    // same line WITHOUT a number is the legacy form of 143 files and means
    // "approved up to the revision the file carries" — the parser normalises it
    // instead of blocking the tree.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir =
        QDir(temp.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));

    QString pinned = requirementContent(QStringLiteral("REQ-SW-PL-910"),
                                        QStringLiteral("Pinned"), QStringLiteral("ACTIVE"),
                                        QStringLiteral("—"), QStringLiteral("—"));
    pinned.replace(QStringLiteral("- **Приоритет:** Low\n"),
                   QStringLiteral("- **Одобрено:** 2026-11-22, потребител (ревизия 2)\n"
                                  "- **Ревизия:** 3\n"
                                  "- **Базова линия:** 2026-11-24, "
                                  "feat/REQ-SW-PL-910-x (ревизия 2)\n"
                                  "- **Приоритет:** Low\n"));
    QVERIFY(!writeFixture(activeDir, QStringLiteral("REQ-SW-PL-910-pinned.md"), pinned).isEmpty());

    QString legacy = requirementContent(QStringLiteral("REQ-SW-PL-911"),
                                        QStringLiteral("Legacy"), QStringLiteral("ACTIVE"),
                                        QStringLiteral("—"), QStringLiteral("—"));
    legacy.replace(QStringLiteral("- **Приоритет:** Low\n"),
                   QStringLiteral("- **Одобрено:** 2026-11-22, потребител\n"
                                  "- **Ревизия:** 4\n"
                                  "- **Приоритет:** Low\n"));
    QVERIFY(!writeFixture(activeDir, QStringLiteral("REQ-SW-PL-911-legacy.md"), legacy).isEmpty());

    QString bare = requirementContent(QStringLiteral("REQ-SW-PL-912"),
                                      QStringLiteral("Bare"), QStringLiteral("BACKLOG"),
                                      QStringLiteral("—"), QStringLiteral("—"));
    bare.replace(QStringLiteral("- **Приоритет:** Low\n"),
                 QStringLiteral("- **Ревизия:** 1\n- **Приоритет:** Low\n"));
    QVERIFY(!writeFixture(activeDir, QStringLiteral("REQ-SW-PL-912-bare.md"), bare).isEmpty());

    const QVector<Requirement> parsed = RequirementsParser::parseDirectory(temp.path());
    const Requirement *pin = findRequirement(parsed, QStringLiteral("REQ-SW-PL-910"));
    QVERIFY(pin);
    QCOMPARE(pin->revision, 3);
    QCOMPARE(pin->approvedRevision, 2);
    QCOMPARE(pin->approvedDate, QStringLiteral("2026-11-22"));
    QCOMPARE(pin->approvedBy, QStringLiteral("потребител"));
    QCOMPARE(pin->baselineDate, QStringLiteral("2026-11-24"));
    QCOMPARE(pin->baselineRef, QStringLiteral("feat/REQ-SW-PL-910-x"));
    QCOMPARE(pin->baselineRevision, 2);

    const Requirement *leg = findRequirement(parsed, QStringLiteral("REQ-SW-PL-911"));
    QVERIFY(leg);
    QCOMPARE(leg->revision, 4);
    QCOMPARE(leg->approvedRevision, 4); // legacy: approved up to the current one
    QVERIFY(leg->baselineDate.isEmpty());
    QCOMPARE(leg->baselineRevision, -1);

    const Requirement *noLine = findRequirement(parsed, QStringLiteral("REQ-SW-PL-912"));
    QVERIFY(noLine);
    QCOMPARE(noLine->approvedRevision, -1); // no approval at all — not "approved"
}

void TestParser::startContractChange_sealsApproval()
{
    // REQ-SW-PL-106: "Промени договора" PINS the old approval instead of
    // erasing it. DRAFT plus "approved revision 2" is true; deleting the line
    // made the file claim nobody ever accepted anything.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir =
        QDir(temp.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));

    QString content = requirementContent(QStringLiteral("REQ-SW-PL-913"),
                                         QStringLiteral("Sealed"), QStringLiteral("ACTIVE"),
                                         QStringLiteral("—"), QStringLiteral("—"));
    content.replace(QStringLiteral("- **Приоритет:** Low\n"),
                    QStringLiteral("- **Одобрено:** 2026-11-21, потребител\n"
                                   "- **Ревизия:** 2\n"
                                   "- **Приоритет:** Low\n"));
    const QString path =
        writeFixture(activeDir, QStringLiteral("REQ-SW-PL-913-sealed.md"), content);
    QVERIFY(!path.isEmpty());

    QVERIFY(RequirementsParser::startContractChange(
        path, QStringLiteral("добавя се AC7"), QStringLiteral("потребител"),
        QStringLiteral("2026-11-22")));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString after = QString::fromUtf8(file.readAll());
    file.close();
    QVERIFY2(after.contains(QStringLiteral("- **Статус:** DRAFT")), qPrintable(after));
    // REQ-SW-PL-109: the seal carries the contract as well, so "approved" says
    // WHICH text the human accepted, not only which number.
    QVERIFY2(after.contains(QStringLiteral("- **Одобрено:** 2026-11-21, потребител "
                                           "(ревизия 2, договор %1)")
                                .arg(RequirementsParser::contractHash(content))),
             qPrintable(after));
    QVERIFY(after.contains(QStringLiteral("- **Ревизия:** 3")));

    const QVector<Requirement> parsed = RequirementsParser::parseDirectory(temp.path());
    const Requirement *req = findRequirement(parsed, QStringLiteral("REQ-SW-PL-913"));
    QVERIFY(req);
    QCOMPARE(req->revision, 3);
    QCOMPARE(req->approvedRevision, 2);
}

void TestParser::approveDraft_namesTheApprovedRevision()
{
    // REQ-SW-PL-106: the row the accept writes bumps the revision, so the number
    // in "Одобрено:" must be the POST-bump one — otherwise the approved
    // requirement reads "revision 2 > approved 1" and blocks itself.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir =
        QDir(temp.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    const QString path = writeFixture(
        activeDir, QStringLiteral("DRAFT-REQ-SW-PL-914-draft.md"),
        requirementContent(QStringLiteral("REQ-SW-PL-914"), QStringLiteral("Draft"),
                           QStringLiteral("DRAFT"), QStringLiteral("—"),
                           QStringLiteral("—")));
    QVERIFY(!path.isEmpty());

    QString error;
    QVERIFY2(RequirementsParser::approveDraft(path, QStringLiteral("2026-11-22"), &error),
             qPrintable(error));

    const QVector<Requirement> parsed = RequirementsParser::parseDirectory(temp.path());
    const Requirement *req = findRequirement(parsed, QStringLiteral("REQ-SW-PL-914"));
    QVERIFY(req);
    QCOMPARE(req->status, QStringLiteral("ACTIVE"));
    QCOMPARE(req->revision, 1);
    QCOMPARE(req->approvedRevision, req->revision);
    QVERIFY(req->rawContent.contains(QStringLiteral("(ревизия 1, договор ")));
    QCOMPARE(req->approvedContractHash, RequirementsParser::contractHash(req->rawContent));
}

void TestParser::appendHistoryRow_keepsRevision()
{
    // REQ-SW-PL-109 AC2: a history row is bookkeeping — the number counts the
    // CONTRACT, so two rows leave it exactly where it was.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir =
        QDir(temp.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    QString fixture = requirementContent(QStringLiteral("REQ-SW-PL-909"),
                                         QStringLiteral("История"),
                                         QStringLiteral("ACTIVE"),
                                         QStringLiteral("—"), QStringLiteral("—"));
    fixture.replace(QStringLiteral("- **Приоритет:** Low\n"),
                    QStringLiteral("- **Ревизия:** 2\n- **Приоритет:** Low\n"));
    QVERIFY(!writeFixture(activeDir, QStringLiteral("REQ-SW-PL-909.md"), fixture).isEmpty());

    QVector<Requirement> parsed = RequirementsParser::parseDirectory(temp.path());
    QCOMPARE(parsed.size(), 1);
    Requirement req = parsed.first();
    RequirementsParser::appendHistoryRow(req, QStringLiteral("2026-11-22"),
                                         QStringLiteral("редакция"), QStringLiteral("Ivan"),
                                         QStringLiteral("правопис"));
    QCOMPARE(req.revision, 2);
    RequirementsParser::appendHistoryRow(req, QStringLiteral("2026-11-23"),
                                         QStringLiteral("редакция"), QStringLiteral("Ivan"),
                                         QStringLiteral("пак правопис"));
    QCOMPARE(req.revision, 2);
    QVERIFY(req.rawContent.contains(QStringLiteral("| 2026-11-23 | редакция | Ivan | пак правопис |")));
    QCOMPARE(req.rawContent.count(QStringLiteral("## История")), 1);
}

void TestParser::contract_statusRowAndTick_keepRevision()
{
    // REQ-SW-PL-109 AC5(а) + AC2: status + history row + ticked AC. The number
    // stays and the contract hash stays — the gate sees the same input.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir =
        QDir(temp.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    QString fixture = requirementContent(QStringLiteral("REQ-SW-PL-920"),
                                         QStringLiteral("Отметки"),
                                         QStringLiteral("ACTIVE"),
                                         QStringLiteral("—"), QStringLiteral("—"));
    fixture.replace(QStringLiteral("- **Приоритет:** Low\n"),
                    QStringLiteral("- **Ревизия:** 2\n- **Приоритет:** Low\n"));
    QVERIFY(!writeFixture(activeDir, QStringLiteral("REQ-SW-PL-920.md"), fixture).isEmpty());

    Requirement req = RequirementsParser::parseDirectory(temp.path()).first();
    const QString contractBefore = RequirementsParser::contractHash(req.rawContent);
    QVERIFY2(!contractBefore.isEmpty(), "title + one AC is a contract");

    req.rawContent.replace(QStringLiteral("- [ ] 1. Критерий."),
                           QStringLiteral("- [x] 1. Критерий."));
    RequirementsParser::setStatusLine(req, QStringLiteral("IMPLEMENTED"));
    RequirementsParser::appendHistoryRow(req, QStringLiteral("2026-11-23"),
                                         QStringLiteral("IMPLEMENTED"),
                                         QStringLiteral("Ivan"),
                                         QStringLiteral("готово"));

    QCOMPARE(RequirementsParser::revisionInContent(req.rawContent), 2);
    QCOMPARE(RequirementsParser::contractHash(req.rawContent), contractBefore);
}

void TestParser::contract_draftAndReapprove_keepRevision()
{
    // REQ-SW-PL-109 AC5(б): ACTIVE → DRAFT → ACTIVE with the contract untouched
    // leaves the number and the seal alone, so the gate stays green.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir =
        QDir(temp.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    QString fixture = requirementContent(QStringLiteral("REQ-SW-PL-921"),
                                         QStringLiteral("Обратно"),
                                         QStringLiteral("ACTIVE"),
                                         QStringLiteral("—"), QStringLiteral("—"));
    fixture.replace(QStringLiteral("- **Приоритет:** Low\n"),
                    QStringLiteral("- **Ревизия:** 2\n- **Приоритет:** Low\n"));
    // The seal is the fixture's own contract, so the fixture is "approved".
    const QString seal = RequirementsParser::contractHash(fixture);
    fixture.replace(QStringLiteral("- **Приоритет:** Low\n"),
                    QStringLiteral("- **Одобрено:** 2026-11-20, потребител "
                                   "(ревизия 2, договор %1)\n- **Приоритет:** Low\n")
                        .arg(seal));
    const QString path = writeFixture(activeDir, QStringLiteral("REQ-SW-PL-921.md"), fixture);
    QVERIFY(!path.isEmpty());
    const QString sealedBefore =
        RequirementsParser::parseDirectory(temp.path()).first().approvedContractHash;
    QVERIFY2(!sealedBefore.isEmpty(), "the fixture is sealed");

    // The human flips it back to DRAFT by hand (the editor path) — a status, not
    // a contract — and accepts it again.
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString text = QString::fromUtf8(file.readAll());
        file.close();
        text.replace(QStringLiteral("- **Статус:** ACTIVE"),
                     QStringLiteral("- **Статус:** DRAFT"));
        QFile out(path);
        QVERIFY(out.open(QIODevice::WriteOnly | QIODevice::Truncate));
        out.write(text.toUtf8());
    }
    QString error;
    QVERIFY2(RequirementsParser::approveDraft(path, QStringLiteral("2026-11-23"), &error),
             qPrintable(error));

    const Requirement after = RequirementsParser::parseDirectory(temp.path()).first();
    QCOMPARE(after.status, QStringLiteral("ACTIVE"));
    QCOMPARE(after.revision, 2);
    QCOMPARE(after.approvedRevision, 2);
    QCOMPARE(after.approvedContractHash, sealedBefore);
    QCOMPARE(RequirementsParser::contractHash(after.rawContent), sealedBefore);
}

void TestParser::contract_acWord_bumpsRevision()
{
    // REQ-SW-PL-109 AC5(в): ONE word in an AC is a contract change — the number
    // becomes N+1 in the same write and the seal still names the old contract,
    // which is exactly what makes the gate ask for a new approval.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir =
        QDir(temp.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    QString fixture = requirementContent(QStringLiteral("REQ-SW-PL-922"),
                                         QStringLiteral("Дума"),
                                         QStringLiteral("ACTIVE"),
                                         QStringLiteral("—"), QStringLiteral("—"));
    fixture.replace(QStringLiteral("- **Приоритет:** Low\n"),
                    QStringLiteral("- **Ревизия:** 2\n- **Приоритет:** Low\n"));
    const QString seal = RequirementsParser::contractHash(fixture);
    fixture.replace(QStringLiteral("- **Приоритет:** Low\n"),
                    QStringLiteral("- **Одобрено:** 2026-11-20, потребител "
                                   "(ревизия 2, договор %1)\n- **Приоритет:** Low\n")
                        .arg(seal));
    const QString path = writeFixture(activeDir, QStringLiteral("REQ-SW-PL-922.md"), fixture);
    QVERIFY(!path.isEmpty());

    const Requirement before = RequirementsParser::parseDirectory(temp.path()).first();
    const QString accepted = before.approvedContractHash;
    QVERIFY(!accepted.isEmpty());

    QString error;
    QVERIFY2(RequirementsParser::startContractChange(
                 path, QStringLiteral("една дума в AC1"), QStringLiteral("потребител"),
                 QStringLiteral("2026-11-23"), &error),
             qPrintable(error));
    // …and the human makes the change the action announced.
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QString edited = QString::fromUtf8(file.readAll());
    file.close();
    edited.replace(QStringLiteral("- [ ] 1. Критерий."),
                   QStringLiteral("- [ ] 1. Критерият."));
    {
        QFile out(path);
        QVERIFY(out.open(QIODevice::WriteOnly | QIODevice::Truncate));
        out.write(edited.toUtf8());
    }

    const Requirement after = RequirementsParser::parseDirectory(temp.path()).first();
    QCOMPARE(after.revision, 3);                 // N+1
    QCOMPARE(after.approvedRevision, 2);         // the seal did not move
    QCOMPARE(after.approvedContractHash, accepted);
    QVERIFY2(RequirementsParser::contractHash(after.rawContent) != accepted,
             "the changed AC is a different contract");
}

void TestParser::contract_technicalDetails_keepRevision()
{
    // REQ-SW-PL-109 AC5(г): a paragraph under "Технически детайли" is not a
    // contract — neither the number nor the hash moves.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir =
        QDir(temp.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    QString fixture = requirementContent(QStringLiteral("REQ-SW-PL-923"),
                                         QStringLiteral("Детайли"),
                                         QStringLiteral("ACTIVE"),
                                         QStringLiteral("—"), QStringLiteral("—"));
    fixture.replace(QStringLiteral("- **Приоритет:** Low\n"),
                    QStringLiteral("- **Ревизия:** 2\n- **Приоритет:** Low\n"));
    fixture += QStringLiteral("\n## Технически детайли\n\n- първи ред\n");
    QVERIFY(!writeFixture(activeDir, QStringLiteral("REQ-SW-PL-923.md"), fixture).isEmpty());

    Requirement req = RequirementsParser::parseDirectory(temp.path()).first();
    const QString contractBefore = RequirementsParser::contractHash(req.rawContent);
    req.rawContent += QStringLiteral("- втори ред, цял абзац\n");
    RequirementsParser::appendHistoryRow(req, QStringLiteral("2026-11-23"),
                                         QStringLiteral("бележка"),
                                         QStringLiteral("Ivan"),
                                         QStringLiteral("уточнение"));

    QCOMPARE(RequirementsParser::revisionInContent(req.rawContent), 2);
    QCOMPARE(RequirementsParser::contractHash(req.rawContent), contractBefore);
}

void TestParser::contract_hashIsPinned()
{
    // The shell gate (scripts/check-req-revision.sh, private repo) recomputes
    // this hash with awk + sha256sum. tests/scripts/test-req-revision.sh pins
    // the SAME fixture and the SAME constant, so the two implementations cannot
    // drift apart without one of the two tests going red (REQ-SW-PL-109 AC1).
    const QString fixture = QStringLiteral(
        "# REQ-SW-PL-930: Печат\n"
        "\n"
        "- **Статус:** ACTIVE\n"
        "- **Родител:** REQ-SW-PL-001\n"
        "- **Зависи от:** REQ-SW-PL-002\n"
        "\n"
        "## Как ще разберем, че работи\n"
        "\n"
        "- [x] първи критерий\n"
        "- [ ] втори критерий\n"
        "\n"
        "## История\n"
        "\n"
        "| дата | промяна | кой | защо |\n"
        "|---|---|---|---|\n"
        "| 2026-11-23 | създадено | Ivan | фикстура |\n");
    QCOMPARE(RequirementsParser::contractHash(fixture), QStringLiteral("e7ebe43e"));
    // A ticked AC is the same contract as an unticked one; a changed word is not.
    QString ticked = fixture;
    ticked.replace(QStringLiteral("- [x] първи критерий"),
                   QStringLiteral("- [ ] първи критерий"));
    QCOMPARE(RequirementsParser::contractHash(ticked),
             RequirementsParser::contractHash(fixture));
    QString worded = fixture;
    worded.replace(QStringLiteral("първи критерий"),
                   QStringLiteral("първият критерий"));
    QVERIFY(RequirementsParser::contractHash(worded)
            != RequirementsParser::contractHash(fixture));
}

void TestParser::findFileById_survivesRename()
{
    // REQ-SW-PL-102 AC7: the H1 is the identity, the path is not.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString activeDir =
        QDir(temp.path()).filePath(QStringLiteral("DevelopmentProcess/requirements/active"));
    const QString original =
        writeFixture(activeDir, QStringLiteral("REQ-SW-PL-910.md"),
                     requirementContent(QStringLiteral("REQ-SW-PL-910"),
                                        QStringLiteral("Преименувано"),
                                        QStringLiteral("DRAFT"), QStringLiteral("—"),
                                        QStringLiteral("—")));
    QVERIFY(!original.isEmpty());

    const QVector<RequirementRoot> roots{RequirementRoot{temp.path()}};
    QCOMPARE(RequirementsParser::findFileById(roots, QStringLiteral("REQ-SW-PL-910")),
             original);

    const QString renamed =
        QDir(activeDir).filePath(QStringLiteral("REQ-SW-PL-910-drugo-ime.md"));
    QVERIFY(QFile::rename(original, renamed));
    QCOMPARE(RequirementsParser::findFileById(roots, QStringLiteral("REQ-SW-PL-910")),
             renamed);
    QVERIFY(RequirementsParser::findFileById(roots, QStringLiteral("REQ-SW-PL-999")).isEmpty());
}

void TestParser::createdAliasDoesNotRewriteTheRealTree()
{
    // REQ-SW-PL-101 AC3: `Създадено:` falls back to `Дата:` as an ALIAS in the
    // parser, never as a migration. Reading the whole real requirement set must
    // leave every file byte-identical — otherwise merely opening the manager
    // would rewrite the 135 legacy files.
    const QStringList roots = RequirementsParser::discoverRepoRoots();
    if (roots.isEmpty())
        QSKIP("no requirement tree next to the test binary");

    QHash<QString, QByteArray> before;
    QVector<RequirementRoot> rootList;
    for (const QString &root : roots) {
        rootList.append(RequirementRoot{root});
        const QString tree =
            QDir(root).filePath(QString::fromUtf8("DevelopmentProcess/requirements"));
        QDirIterator it(tree, QStringList() << QStringLiteral("*.md"),
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly))
                continue;
            before.insert(path, file.readAll());
        }
    }
    if (before.size() < 10)
        QSKIP("the requirement tree is too small to be the real one");

    const QVector<Requirement> parsed = RequirementsParser::parseDirectories(rootList);
    QVERIFY2(parsed.size() >= 10, "the real tree must parse to a real requirement set");

    // The alias is visible: a legacy file with only `Дата:` gets the same value
    // in `created` — no file is touched to achieve that.
    int legacyChecked = 0;
    for (const Requirement &req : parsed) {
        if (req.rawContent.contains(QStringLiteral("- **Създадено:**")))
            continue;
        if (!req.rawContent.contains(QStringLiteral("- **Дата:**")))
            continue;
        if (req.date.isEmpty())
            continue;
        if (req.created == req.date)
            ++legacyChecked;
    }
    QVERIFY2(legacyChecked > 0,
             "a legacy `Дата:`-only file must read its creation date through the alias");

    int rewritten = 0;
    for (auto it = before.constBegin(); it != before.constEnd(); ++it) {
        QFile file(it.key());
        if (!file.open(QIODevice::ReadOnly)) {
            ++rewritten;
            continue;
        }
        if (file.readAll() != it.value())
            ++rewritten;
    }
    QCOMPARE(rewritten, 0);
}

// No QTEST_GUILESS_MAIN here: the three test classes share one binary whose
// main lives in test_main.cpp.