#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include "RequirementsParser.h"

namespace Daqster {

/**
 * @brief Full-text search engine over parsed requirements (REQ-SW-PL-011).
 *
 * Performs case-insensitive, whitespace-split multi-term search with AND
 * semantics (every term must match). Terms may be restricted to a single
 * structured field with a prefix syntax: `id:`, `status:`, `priority:`,
 * `assignee:`, `repo:`, `section:` — the term is split on the FIRST colon
 * and only known keys are treated as field filters; any other term (e.g.
 * `http://`, `REQ-SW-PL-001`) falls back to full-text matching.
 *
 * The engine consumes ONLY the structured, normalized Requirement records
 * produced by RequirementsParser — it never re-parses raw Markdown and does
 * not index `rawContent` or `dependencyHints` (architecture rule, REQ-SW-PL-011
 * AC4). Per call it builds one lowercased normalized index blob per
 * requirement in a single pass, which keeps lookups fast on large bases.
 *
 * QtCore-only (no Q_OBJECT) so it can be unit-tested headlessly, mirroring
 * RequirementsValidator.
 */
class RequirementsSearchEngine
{
public:
    /**
     * @brief Returns the requirements matching @p query.
     *
     * An empty/blank query returns @p requirements UNCHANGED (the same
     * implicitly-shared vector). A non-empty query is whitespace-split into
     * terms that must ALL match (AND semantics). Matching is case-insensitive
     * against the normalized text of each requirement; terms with a known
     * field prefix (`id:`, `status:`, `priority:`, `assignee:`, `repo:`,
     * `section:`) are matched against that field only.
     */
    static QVector<Requirement> filter(const QVector<Requirement> &requirements,
                                       const QString &query);

    /**
     * @brief Splits @p query into search terms on whitespace runs.
     */
    static QStringList tokenize(const QString &query);

    /**
     * @brief Returns the lowercased, normalized searchable text of @p req.
     *
     * Contains id, title, description, acceptance criteria, traceability,
     * commits, code, tests, parentId, dependencies, status, priority,
     * assignee, repo, section, fileName and date — joined with single spaces.
     * Deliberately EXCLUDES rawContent and dependencyHints.
     */
    static QString normalizedText(const Requirement &req);
};

} // namespace Daqster
