#include <QtTest>

#include "RequirementsSearchEngine.h"
#include "RequirementsParser.h"
#include "test_search.h"

using namespace Daqster;

namespace {

// Minimal searchable Requirement record. Callers override the fields they
// care about per test.
Requirement makeRequirement(const QString &id, const QString &section)
{
    Requirement req;
    req.id = id;
    req.title = id;
    req.status = QStringLiteral("ACTIVE");
    req.priority = QStringLiteral("Medium");
    req.assignee = QStringLiteral("Architect");
    req.repo = QStringLiteral("public");
    req.section = section;
    req.description = QStringLiteral("Description");
    req.date = QStringLiteral("2026-07-31");
    req.fileName = id + QStringLiteral(".md");
    req.acceptanceCriteria << QStringLiteral("1. Criterion");
    req.traceability = QStringLiteral("Traceability text");
    return req;
}

} // namespace

void TestSearchEngine::emptyQueryReturnsAll()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-003"), QStringLiteral("archive")));

    const QVector<Requirement> emptyResult =
        RequirementsSearchEngine::filter(reqs, QString());
    const QVector<Requirement> blankResult =
        RequirementsSearchEngine::filter(reqs, QStringLiteral("   "));

    // Empty/blank query returns the input unchanged (same size and content).
    QCOMPARE(emptyResult.size(), reqs.size());
    QCOMPARE(blankResult.size(), reqs.size());
    for (int i = 0; i < reqs.size(); ++i) {
        QCOMPARE(emptyResult.at(i).id, reqs.at(i).id);
        QCOMPARE(blankResult.at(i).id, reqs.at(i).id);
    }
}

void TestSearchEngine::tokenize_whitespaceRuns()
{
    const QStringList tokens =
        RequirementsSearchEngine::tokenize(QStringLiteral("a  b\t c\n d"));
    QCOMPARE(tokens.size(), 4);
    QCOMPARE(tokens.at(0), QStringLiteral("a"));
    QCOMPARE(tokens.at(1), QStringLiteral("b"));
    QCOMPARE(tokens.at(2), QStringLiteral("c"));
    QCOMPARE(tokens.at(3), QStringLiteral("d"));

    QVERIFY(RequirementsSearchEngine::tokenize(QString()).isEmpty());
    QVERIFY(RequirementsSearchEngine::tokenize(QStringLiteral(" \t ")).isEmpty());
}

void TestSearchEngine::fulltext_title()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active")));
    reqs[0].title = QStringLiteral("Alpha Processor");
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active")));
    reqs[1].title = QStringLiteral("Beta Engine");

    const QVector<Requirement> matches =
        RequirementsSearchEngine::filter(reqs, QStringLiteral("alpha"));

    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.at(0).id, QStringLiteral("REQ-SW-PL-001"));
}

void TestSearchEngine::fulltext_description()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active")));
    reqs[0].description = QStringLiteral("Handles the gamma workflow.");
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active")));

    const QVector<Requirement> matches =
        RequirementsSearchEngine::filter(reqs, QStringLiteral("gamma"));

    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.at(0).id, QStringLiteral("REQ-SW-PL-001"));
}

void TestSearchEngine::fulltext_acceptanceCriteria()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active")));
    reqs[0].acceptanceCriteria.clear();
    reqs[0].acceptanceCriteria << QStringLiteral("1. Delta criterion");
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active")));

    const QVector<Requirement> matches =
        RequirementsSearchEngine::filter(reqs, QStringLiteral("delta"));

    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.at(0).id, QStringLiteral("REQ-SW-PL-001"));
}

void TestSearchEngine::fulltext_traceability()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active")));
    reqs[0].traceability = QStringLiteral("Epsilon commit log");
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active")));

    const QVector<Requirement> matches =
        RequirementsSearchEngine::filter(reqs, QStringLiteral("epsilon"));

    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.at(0).id, QStringLiteral("REQ-SW-PL-001"));
}

void TestSearchEngine::multiTerm_andSemantics()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active")));
    reqs[0].title = QStringLiteral("Alpha Processor");
    reqs[0].description = QStringLiteral("with beta support");
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active")));
    reqs[1].title = QStringLiteral("Alpha Processor");
    reqs[1].description = QStringLiteral("standalone");
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-003"), QStringLiteral("active")));
    reqs[2].title = QStringLiteral("Beta Engine");

    // AND semantics: every term must match. Only REQ-SW-PL-001 has both.
    const QVector<Requirement> matches =
        RequirementsSearchEngine::filter(reqs, QStringLiteral("alpha beta"));

    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.at(0).id, QStringLiteral("REQ-SW-PL-001"));
}

void TestSearchEngine::caseInsensitive()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active")));
    reqs[0].title = QStringLiteral("SEARCH");
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active")));

    const QVector<Requirement> matches =
        RequirementsSearchEngine::filter(reqs, QStringLiteral("search"));

    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.at(0).id, QStringLiteral("REQ-SW-PL-001"));
}

void TestSearchEngine::noMatchReturnsEmpty()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active")));

    const QVector<Requirement> matches =
        RequirementsSearchEngine::filter(reqs, QStringLiteral("xyzzy_nonexistent_term"));

    QVERIFY(matches.isEmpty());
}

void TestSearchEngine::fieldPrefix_id()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active")));
    // The value "REQ-SW-PL-001" also appears in this requirement's title: a
    // field-restricted "id:" search must still match only the real ID owner.
    reqs[1].title = QStringLiteral("mentions REQ-SW-PL-001 in the title");

    const QVector<Requirement> matches =
        RequirementsSearchEngine::filter(reqs, QStringLiteral("id:REQ-SW-PL-001"));
    const QVector<Requirement> lowerMatches =
        RequirementsSearchEngine::filter(reqs, QStringLiteral("id:req-sw-pl-001"));

    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.at(0).id, QStringLiteral("REQ-SW-PL-001"));
    QCOMPARE(lowerMatches.size(), 1);
    QCOMPARE(lowerMatches.at(0).id, QStringLiteral("REQ-SW-PL-001"));
}

void TestSearchEngine::fieldPrefix_status()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active")));
    reqs[0].status = QStringLiteral("ACTIVE");
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active")));
    reqs[1].status = QStringLiteral("DONE");

    // Both requirements have section "active" - only the status field may
    // decide, proving "status:" is field-restricted.
    const QVector<Requirement> matches =
        RequirementsSearchEngine::filter(reqs, QStringLiteral("status:active"));

    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.at(0).id, QStringLiteral("REQ-SW-PL-001"));
}

void TestSearchEngine::fieldPrefix_priority()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active")));
    reqs[0].priority = QStringLiteral("High");
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active")));
    reqs[1].priority = QStringLiteral("Low");

    const QVector<Requirement> matches =
        RequirementsSearchEngine::filter(reqs, QStringLiteral("priority:high"));

    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.at(0).id, QStringLiteral("REQ-SW-PL-001"));
}

void TestSearchEngine::fieldPrefix_assignee()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active")));
    reqs[0].assignee = QStringLiteral("Architect");
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active")));
    reqs[1].assignee = QStringLiteral("Implementation");

    const QVector<Requirement> matches =
        RequirementsSearchEngine::filter(reqs, QStringLiteral("assignee:architect"));

    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.at(0).id, QStringLiteral("REQ-SW-PL-001"));
}

void TestSearchEngine::fieldPrefix_repo()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active")));
    reqs[0].repo = QStringLiteral("public");
    reqs.append(makeRequirement(QStringLiteral("REQ-PLG-002"), QStringLiteral("active")));
    reqs[1].repo = QStringLiteral("private");

    const QVector<Requirement> matches =
        RequirementsSearchEngine::filter(reqs, QStringLiteral("repo:public"));

    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.at(0).id, QStringLiteral("REQ-SW-PL-001"));
}

void TestSearchEngine::fieldPrefix_section()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active")));
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("archive")));

    const QVector<Requirement> matches =
        RequirementsSearchEngine::filter(reqs, QStringLiteral("section:archive"));

    QCOMPARE(matches.size(), 1);
    QCOMPARE(matches.at(0).id, QStringLiteral("REQ-SW-PL-002"));
}

void TestSearchEngine::unknownPrefix_fallsBackToFulltext()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active")));
    reqs[0].description = QStringLiteral("see http://example.com for details");
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active")));

    // "http://" is not a known field key -> full-text search for the literal.
    const QVector<Requirement> httpMatches =
        RequirementsSearchEngine::filter(reqs, QStringLiteral("http://example.com"));
    QCOMPARE(httpMatches.size(), 1);
    QCOMPARE(httpMatches.at(0).id, QStringLiteral("REQ-SW-PL-001"));

    // "status:" has an EMPTY value -> not a field filter; the literal
    // "status:" text must be matched via full-text (only reqs[1] contains it,
    // even though reqs[0].status == "ACTIVE").
    reqs[1].description = QStringLiteral("contains status: literal marker");
    const QVector<Requirement> emptyValueMatches =
        RequirementsSearchEngine::filter(reqs, QStringLiteral("status:"));
    QCOMPARE(emptyValueMatches.size(), 1);
    QCOMPARE(emptyValueMatches.at(0).id, QStringLiteral("REQ-SW-PL-002"));
}

void TestSearchEngine::relation_search()
{
    QVector<Requirement> reqs;
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active")));
    reqs[0].parentId = QStringLiteral("REQ-SW-PL-016");
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-002"), QStringLiteral("active")));
    reqs[1].dependencies << QStringLiteral("REQ-SW-PL-016");
    reqs.append(makeRequirement(QStringLiteral("REQ-SW-PL-003"), QStringLiteral("active")));

    // A bare ID term matches any requirement whose parentId or dependencies
    // reference it (the reference fields are part of the normalized blob).
    const QVector<Requirement> matches =
        RequirementsSearchEngine::filter(reqs, QStringLiteral("REQ-SW-PL-016"));

    QCOMPARE(matches.size(), 2);
    QVERIFY(matches.at(0).id == QStringLiteral("REQ-SW-PL-001")
            || matches.at(0).id == QStringLiteral("REQ-SW-PL-002"));
    QVERIFY(matches.at(1).id == QStringLiteral("REQ-SW-PL-001")
            || matches.at(1).id == QStringLiteral("REQ-SW-PL-002"));
    QVERIFY(matches.at(0).id != matches.at(1).id);
}

void TestSearchEngine::normalizedText_containsAllFields()
{
    Requirement req = makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active"));
    req.title = QStringLiteral("Search Engine");
    req.status = QStringLiteral("ACTIVE");
    req.priority = QStringLiteral("High");
    req.assignee = QStringLiteral("Architect");
    req.repo = QStringLiteral("public");
    req.section = QStringLiteral("active");
    req.fileName = QStringLiteral("REQ-SW-PL-001-search-engine.md");
    req.date = QStringLiteral("2026-07-31");
    req.parentId = QStringLiteral("REQ-SW-PL-000");
    req.dependencies << QStringLiteral("REQ-SW-PL-002");
    req.commits = QStringLiteral("abc123");
    req.code = QStringLiteral("src/plugins/");
    req.tests = QStringLiteral("tests/");

    const QString blob = RequirementsSearchEngine::normalizedText(req);

    QVERIFY2(blob.contains(QStringLiteral("req-sw-pl-001")), qPrintable(blob));
    QVERIFY2(blob.contains(QStringLiteral("search engine")), qPrintable(blob));
    QVERIFY2(blob.contains(QStringLiteral("active")), qPrintable(blob));
    QVERIFY2(blob.contains(QStringLiteral("high")), qPrintable(blob));
    QVERIFY2(blob.contains(QStringLiteral("architect")), qPrintable(blob));
    QVERIFY2(blob.contains(QStringLiteral("public")), qPrintable(blob));
    QVERIFY2(blob.contains(QStringLiteral("req-sw-pl-001-search-engine.md")), qPrintable(blob));
    QVERIFY2(blob.contains(QStringLiteral("2026-07-31")), qPrintable(blob));
}

void TestSearchEngine::normalizedText_excludesRawContentAndHints()
{
    Requirement req = makeRequirement(QStringLiteral("REQ-SW-PL-001"), QStringLiteral("active"));
    req.rawContent = QStringLiteral("SENTINEL_RAW_CONTENT");
    req.dependencyHints.insert(QStringLiteral("REQ-SW-PL-999"),
                               QStringLiteral("SENTINEL_HINT"));

    const QString blob = RequirementsSearchEngine::normalizedText(req);

    QVERIFY(!blob.contains(QStringLiteral("SENTINEL_RAW_CONTENT")));
    QVERIFY(!blob.contains(QStringLiteral("SENTINEL_HINT")));
}

// No QTEST_GUILESS_MAIN here: the Requirements Manager test classes share one
// binary whose main lives in test_main.cpp.
