#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include "RequirementsParser.h"
#include "test_merge.h"

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

// Returns the active requirements directory for a root.
QString activeDirOf(const QString &root)
{
    return QDir(root).filePath(QString::fromUtf8(kRequirementsSubdir) +
                               QStringLiteral("/active"));
}

QString requirementContent(const QString &id, const QString &title,
                           const QString &parentId, const QString &deps)
{
    return QStringLiteral("# %1: %2\n"
                          "\n"
                          "- **Статус:** ACTIVE\n"
                          "- **Приоритет:** Medium\n"
                          "- **Отговорник (роля):** Implementation\n"
                          "- **Дата:** 2026-07-31\n"
                          "- **Родител:** %3\n"
                          "- **Зависи от:** %4\n"
                          "\n"
                          "## Описание\n"
                          "\n"
                          "Описание на изискването.\n"
                          "\n"
                          "## Acceptance Criteria\n"
                          "\n"
                          "- [ ] 1. Критерий.\n")
        .arg(id, title, parentId, deps);
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

void TestMerge::repoForId_publicPrivateOther()
{
    QCOMPARE(RequirementsParser::repoForId(QStringLiteral("REQ-SW-PL-001")),
             QStringLiteral("public"));
    QCOMPARE(RequirementsParser::repoForId(QStringLiteral("REQ-AI-006")),
             QStringLiteral("private"));
    QCOMPARE(RequirementsParser::repoForId(QStringLiteral("REQ-PLG-004")),
             QStringLiteral("private"));
    QCOMPARE(RequirementsParser::repoForId(QStringLiteral("BOGUS")),
             QStringLiteral("other"));
    // Prefix matching is case-insensitive.
    QCOMPARE(RequirementsParser::repoForId(QStringLiteral("REQ-sw-pl-002")),
             QStringLiteral("public"));
}

void TestMerge::parseDirectories_twoRoots_mergeAndRepoStamp()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString rootA = QDir(temp.path()).filePath(QStringLiteral("rootA"));
    const QString rootB = QDir(temp.path()).filePath(QStringLiteral("rootB"));

    writeFixture(activeDirOf(rootA), QStringLiteral("REQ-SW-PL-001.md"),
                 requirementContent(QStringLiteral("REQ-SW-PL-001"),
                                    QStringLiteral("Public One"),
                                    QStringLiteral("—"), QStringLiteral("—")));
    writeFixture(activeDirOf(rootB), QStringLiteral("REQ-AI-006.md"),
                 requirementContent(QStringLiteral("REQ-AI-006"),
                                    QStringLiteral("Private Six"),
                                    QStringLiteral("—"), QStringLiteral("—")));

    const QVector<Requirement> merged = RequirementsParser::parseDirectories(
        {RequirementRoot{rootA}, RequirementRoot{rootB}});

    QCOMPARE(merged.size(), 2);

    const Requirement *pub = findRequirement(merged, QStringLiteral("REQ-SW-PL-001"));
    QVERIFY2(pub, "REQ-SW-PL-001 should be merged");
    QCOMPARE(pub->repo, QStringLiteral("public"));

    const Requirement *priv = findRequirement(merged, QStringLiteral("REQ-AI-006"));
    QVERIFY2(priv, "REQ-AI-006 should be merged");
    QCOMPARE(priv->repo, QStringLiteral("private"));
}

void TestMerge::parseDirectories_sameFileViaTwoRoots_dedup()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString rootA = QDir(temp.path()).filePath(QStringLiteral("rootA"));
    // root2 points INTO rootA's requirements tree: the same physical file is
    // reachable from both roots and must be parsed exactly once.
    const QString root2 = QDir(rootA).filePath(QString::fromUtf8(kRequirementsSubdir));

    writeFixture(activeDirOf(rootA), QStringLiteral("REQ-SW-PL-001.md"),
                 requirementContent(QStringLiteral("REQ-SW-PL-001"),
                                    QStringLiteral("One"),
                                    QStringLiteral("—"), QStringLiteral("—")));

    const QVector<Requirement> merged = RequirementsParser::parseDirectories(
        {RequirementRoot{rootA}, RequirementRoot{root2}});

    QCOMPARE(merged.size(), 1);
    QCOMPARE(merged.first().id, QStringLiteral("REQ-SW-PL-001"));
    QCOMPARE(merged.first().repo, QStringLiteral("public"));
}

void TestMerge::parseDirectories_stableSort()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString rootA = QDir(temp.path()).filePath(QStringLiteral("rootA"));
    const QString rootB = QDir(temp.path()).filePath(QStringLiteral("rootB"));

    const auto write = [&](const QString &root, const QString &id) {
        writeFixture(activeDirOf(root), id + QStringLiteral(".md"),
                     requirementContent(id, id, QStringLiteral("—"),
                                        QStringLiteral("—")));
    };

    // Root A: interleaved public/private, root B adds more. REQ-SW-PL-004
    // exists in BOTH roots (different files, same bare ID) - both must survive
    // (no ID-based dedup) and sort stably (root A first).
    write(rootA, QStringLiteral("REQ-AI-001"));
    write(rootA, QStringLiteral("REQ-SW-PL-001"));
    write(rootA, QStringLiteral("REQ-SW-PL-004"));
    write(rootA, QStringLiteral("REQ-SW-PL-002"));
    write(rootB, QStringLiteral("REQ-AI-003"));
    write(rootB, QStringLiteral("REQ-SW-PL-004"));
    write(rootB, QStringLiteral("REQ-SW-PL-003"));

    const QVector<Requirement> merged = RequirementsParser::parseDirectories(
        {RequirementRoot{rootA}, RequirementRoot{rootB}});

    const QStringList expectedOrder = {
        QStringLiteral("REQ-AI-001"),
        QStringLiteral("REQ-AI-003"),
        QStringLiteral("REQ-SW-PL-001"),
        QStringLiteral("REQ-SW-PL-002"),
        QStringLiteral("REQ-SW-PL-003"),
        QStringLiteral("REQ-SW-PL-004"), // root A copy
        QStringLiteral("REQ-SW-PL-004"), // root B copy (stable)
    };

    QCOMPARE(merged.size(), 7);
    for (int i = 0; i < merged.size(); ++i)
        QCOMPARE(merged.at(i).id, expectedOrder.at(i));

    // Repo stamps follow the ID prefix (REQ-SW-* -> public, REQ-* -> private).
    QCOMPARE(merged.at(0).repo, QStringLiteral("private"));
    QCOMPARE(merged.at(1).repo, QStringLiteral("private"));
    QCOMPARE(merged.at(2).repo, QStringLiteral("public"));
    QCOMPARE(merged.at(5).repo, QStringLiteral("public"));
    QCOMPARE(merged.at(6).repo, QStringLiteral("public"));
    // The two same-ID entries must not be merged (canonical FILE path dedup
    // only) - both physical files survive.
    QVERIFY(merged.at(5).filePath != merged.at(6).filePath);
}

void TestMerge::parseDirectories_emptyRoots()
{
    // No roots at all -> empty result.
    QVERIFY(RequirementsParser::parseDirectories({}).isEmpty());

    // A root without any requirement files -> empty result.
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QVERIFY(RequirementsParser::parseDirectories(
                {RequirementRoot{temp.path()}}).isEmpty());

    // A non-existent root -> empty result, no crash.
    QVERIFY(RequirementsParser::parseDirectories(
                {RequirementRoot{QStringLiteral("/nonexistent/root")}}).isEmpty());
}

void TestMerge::parseDirectories_dependencyHintsPreserved()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString rootA = QDir(temp.path()).filePath(QStringLiteral("rootA"));

    writeFixture(activeDirOf(rootA), QStringLiteral("REQ-SW-PL-012.md"),
                 requirementContent(QStringLiteral("REQ-SW-PL-012"),
                                    QStringLiteral("Merged Hints"),
                                    QStringLiteral("REQ-SW-PL-013 (публично)"),
                                    QStringLiteral("REQ-PLG-004 (частно)")));

    const QVector<Requirement> merged = RequirementsParser::parseDirectories(
        {RequirementRoot{rootA}});

    QCOMPARE(merged.size(), 1);
    const Requirement *req = findRequirement(merged, QStringLiteral("REQ-SW-PL-012"));
    QVERIFY2(req, "REQ-SW-PL-012 should be merged");

    QCOMPARE(req->parentId, QStringLiteral("REQ-SW-PL-013"));
    QCOMPARE(req->dependencies.size(), 1);
    QCOMPARE(req->dependencies.at(0), QStringLiteral("REQ-PLG-004"));

    QCOMPARE(req->dependencyHints.size(), 2);
    QCOMPARE(req->dependencyHints.value(QStringLiteral("REQ-SW-PL-013")),
             QStringLiteral("публично"));
    QCOMPARE(req->dependencyHints.value(QStringLiteral("REQ-PLG-004")),
             QStringLiteral("частно"));
}

// No QTEST_GUILESS_MAIN here: the Requirements Manager test classes share one
// binary whose main lives in test_main.cpp.
