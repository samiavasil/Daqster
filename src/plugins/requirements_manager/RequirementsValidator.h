#pragma once

#include <QString>
#include <QVector>
#include <optional>
#include "RequirementsParser.h"

namespace Daqster {

/**
 * @brief Consistency validation engine for parsed requirements.
 *
 * Detects dangling references in "Родител:" / "Зависи от:" / "Деца:" metadata,
 * cycles in the dependency and parent graphs (DFS back-edge detection),
 * missing required fields, an invalid or unreadable status, a status that
 * contradicts the file's zone (ACTIVE in archive/, DONE in active/), a
 * DONE requirement with unaddressed acceptance criteria, and a file written
 * with a non-canonical template generation (v0/v1 — a Warning; a file with no
 * description heading at all is an Error).
 *
 * @par Dependency grading (REQ-SW-PL-008)
 * A "Зависи от:" reference is graded by its target's state, not by whether the
 * target is archived: DONE and IMPLEMENTED are silent (normal end states),
 * CANCELLED is a Warning ("зависи от отказано изискване"), an archived target
 * whose status cannot be read is a Warning naming its file, and an ID that does
 * not exist in the tree is an Error ("зависи от несъществуващо изискване") —
 * a broken reference is never silence.
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
     * DRAFT, ACTIVE, IMPLEMENTED, DONE, CANCELLED, BACKLOG. IMPLEMENTED means
     * "the code is on a branch, nothing in develop yet" — a real state, not a
     * typo.
     *
     * @return The allowed status tokens, in the CLI's order.
     */
    static QStringList statusVocabulary();

    /**
     * @brief Severity of a cited code path that does not resolve, by status.
     *
     * A path in "Код:" / "Тестове:" / "Документация:" is a claim that the file
     * exists — but only an implemented requirement makes that claim. An
     * accepted requirement may cite a file it is about to add (REQ-SW-PL-101/102
     * cite the guard scripts they themselves propose), so resolving it would
     * report a plan as a defect.
     *
     * This mirrors the rule in `scripts/validate-traceability.sh` (§6, REQ-SW-PL-103):
     * IMPLEMENTED/DONE -> Error, ACTIVE/BACKLOG -> Warning, DRAFT -> nothing
     * (a draft is a proposal; its files may not exist at all).
     *
     * @param rawStatus The "- **Статус:**" value, verbatim.
     * @return The severity, or std::nullopt when the status does not grade
     *         paths at all (DRAFT, or an unreadable status).
     */
    static std::optional<Severity> pathSeverityForStatus(const QString &rawStatus);

    /**
     * @brief Checks the code paths a requirement cites in its traceability fields.
     *
     * The cited paths are the backticked tokens of "Код:", "Тестове:" and
     * "Документация:" that start with a repository top-level directory, with
     * the same filters the CLI applies (no prose, no globs, no `..`, line
     * ranges stripped, `{a,b}` expanded). Each unresolved token is graded by
     * pathSeverityForStatus().
     *
     * @par Why this is not part of validate()
     * The manager is the authority for fields; repository-wide path resolution
     * is the traceability CLI's job (see the class documentation). This entry
     * point exists so the rule is shared code with a test rather than a
     * second, silent opinion inside the report.
     *
     * @param requirements Parsed requirements.
     * @param repoRoots Repository roots to resolve against. Empty means "derive
     *        them from each requirement's file path plus its sibling repos".
     * @return Issues with field "code", sorted errors first.
     */
    static QVector<Issue> validateCitedPaths(const QVector<Requirement> &requirements,
                                             const QStringList &repoRoots = QStringList());

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
