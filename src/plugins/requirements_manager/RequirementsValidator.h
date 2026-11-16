#pragma once

#include <QString>
#include <QVector>
#include "RequirementsParser.h"

namespace Daqster {

/**
 * @brief Consistency validation engine for parsed requirements.
 *
 * Detects dangling references in "Родител:" / "Зависи от:" / "Деца:" metadata,
 * cycles in the dependency and parent graphs (DFS back-edge detection),
 * missing required fields, an invalid or unreadable status, a status that
 * contradicts the file's zone (ACTIVE in archive/, DONE in active/), and a
 * DONE requirement with unaddressed acceptance criteria.
 *
 * @par Division of labour with the traceability CLI
 * This is the authority for requirement FIELDS — id, status, zone, acceptance
 * criteria, dependencies. Traceability that needs the repository (matrix
 * coverage, commit hashes, code paths, cross-references) belongs to
 * `scripts/validate-traceability.sh`. Both read the same directory scope
 * (DevelopmentProcess/requirements) and the same status vocabulary; neither
 * may invent a third answer.
 *
 * The validator only consumes the structured, normalized Requirement
 * records produced by RequirementsParser — it never re-parses raw
 * Markdown (see REQ-SW-PL-002 AC6-7 / REQ-SW-PL-011). Its scope is therefore
 * whatever the parser walked: RequirementsParser scans the requirements tree
 * (DevelopmentProcess/requirements: active + archive), the same scope the
 * traceability CLI validates — never the whole repo root.
 */
class RequirementsValidator
{
public:
    enum class Severity {
        Error,   //!< breaks consistency (dangling ref, cycle, ...)
        Warning  //!< missing/incomplete field, questionable link
    };

    struct Issue
    {
        QString id;      //!< requirement ID the issue belongs to (may be empty for global issues)
        QString field;   //!< e.g. "parentId", "dependencies", "status"
        QString message; //!< human readable description
        Severity severity = Severity::Error;
        QString filePath; //!< .md file the requirement lives in; empty for global issues
        int line = 0;     //!< line inside @c filePath; 0 = unknown (the parser
                          //!< keeps no line metadata, so it stays 0 today)
    };

    /**
     * @brief Validates a set of parsed requirements.
     * @return Issues sorted by severity (errors first), then by ID.
     */
    static QVector<Issue> validate(const QVector<Requirement> &requirements);

    /**
     * @brief Convenience count of issues of a given severity.
     */
    static int countSeverity(const QVector<Issue> &issues, Severity severity);

    /**
     * @brief The single status vocabulary shared with the traceability CLI.
     *
     * Mirrors `VOCAB` in `scripts/validate-traceability.sh` (private repo):
     * ACTIVE, IMPLEMENTED, DONE, CANCELLED, BACKLOG. IMPLEMENTED means "the code
     * is on a branch, nothing in develop yet" — a real state, not a typo.
     *
     * @return The allowed status tokens, in the CLI's order.
     */
    static QStringList statusVocabulary();

    /**
     * @brief Normalizes a "- **Статус:**" value to a bare vocabulary token.
     *
     * Handles the two conventions the CLI handles: a status carrying a written
     * rationale ("ACTIVE — реализиран на клон `feat/…`", emphasis, backticks)
     * and `DONE НА КЛОНА`, which maps to IMPLEMENTED because the code is on a
     * branch and nothing is in develop.
     *
     * @param rawStatus The parsed status value, verbatim.
     * @return The first all-caps token of the value, or an empty string when
     *         the value carries no readable status.
     */
    static QString normalizedStatus(const QString &rawStatus);
};

} // namespace Daqster
