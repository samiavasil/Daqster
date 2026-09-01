#pragma once

#include <QtTest>

// Test class for RequirementsSearchEngine (REQ-SW-PL-011). Declared in a
// header (instead of relying on QTEST_GUILESS_MAIN inside the .cpp) so the
// Requirements Manager test classes share a single test binary - see
// test_main.cpp.
class TestSearchEngine : public QObject
{
    Q_OBJECT

private slots:
    void emptyQueryReturnsAll();
    void tokenize_whitespaceRuns();
    void fulltext_title();
    void fulltext_description();
    void fulltext_acceptanceCriteria();
    void fulltext_traceability();
    void multiTerm_andSemantics();
    void caseInsensitive();
    void noMatchReturnsEmpty();
    void fieldPrefix_id();
    void fieldPrefix_status();
    void fieldPrefix_priority();
    void fieldPrefix_assignee();
    void fieldPrefix_repo();
    void fieldPrefix_section();
    void unknownPrefix_fallsBackToFulltext();
    void relation_search();
    void normalizedText_containsAllFields();
    void normalizedText_excludesRawContentAndHints();
};
