#pragma once

#include <QString>
#include <QVector>
#include "RequirementsParser.h"

namespace Daqster {

/**
 * @brief Consistency validation engine for parsed requirements.
 *
 * Detects dangling references in "Родител:" / "Зависи от:" metadata,
 * cycles in the dependency and parent graphs (DFS back-edge detection)
 * and missing required fields.
 *
 * The validator only consumes the structured, normalized Requirement
 * records produced by RequirementsParser — it never re-parses raw
 * Markdown (see REQ-SW-002 AC6-7 / REQ-SW-011).
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
};

} // namespace Daqster
