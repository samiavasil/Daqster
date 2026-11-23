#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include "RequirementsParser.h"

namespace Daqster {

/**
 * @brief The preconditions of `DONE`, in ONE place (REQ-SW-PL-107).
 *
 * `DONE` is the owner's verdict; no agent gives it. What an agent CAN do is
 * refuse to record it while something is missing, and say the whole list at
 * once: an unchecked acceptance criterion, no review record (or a record whose
 * last verdict does not accept), and code that is not in `develop` yet.
 *
 * @par Two halves, because one of them is a process
 * The rule is one list, but it is filled in two steps. The file half
 * (`evaluateLocal()`) reads the criteria and the review record — files, no
 * process. The `git` half is a process, and a wait inside the plugin is
 * forbidden (`check_no_event_loops.cmake`: a nested loop re-enters the graph),
 * so `DoneGateCheck` asks git asynchronously and `applyCodeAnswer()` records the
 * answer. A cited path therefore starts out **pending**, and pending refuses:
 * it never reads as "the code is in develop".
 *
 * @par One decision, two callers
 * `missingPreconditions()` is the whole rule; `isDoneCandidate()` is "the list
 * is empty and the status is IMPLEMENTED". The widget's action asks the first
 * (it must name what is missing), the tree badge asks the second — neither has
 * its own copy of the rule, so the row marked „кандидат“ is exactly the row the
 * action would accept.
 *
 * @par Computed, never recorded
 * The fields it fills (`allCriteriaDone`, `reviewRecord`, `reviewVerdict`,
 * `acceptingReview`, `codeInDevelop`, `missingDevelopPaths`, `codeBranch`,
 * `codeCheckError`, `codeCheckPending`, `doneCandidate`) are members of
 * Requirement but no line of the .md file, no status and no new field on disk:
 * `git status` does not change by looking.
 *
 * @par A check that cannot run is a refusal
 * A cited path is looked for in EVERY loaded repo's `develop` (the private repo
 * and `../daqster` carry different halves of the tree, so one of them alone
 * calls a public path "not in develop"). When no repo can answer — no repo, no
 * `develop` branch, no git — `codeInDevelop` stays false and `codeCheckError`
 * names the reason: "cannot check" never answers like "checked, all there".
 *
 * @par Cost
 * The tree evaluates the file half once per requirement per reload; git is
 * asked once per REPO for the whole tree (`git cat-file --batch-check` reads
 * every cited path in one process), and never while painting.
 *
 * @par One vocabulary
 * The verdicts are the four of `plans/reviews/README.md`
 * (`ПРИЕТО`, `ПРИЕТО С БЕЛЕЖКИ`, `ВЪРНАТО`, `ESCALATED <причина>`); the path
 * extraction is RequirementsValidator::citedCodePaths(), the same rule the
 * traceability CLI applies. A second name for the same thing is a defect.
 */
class DoneGate
{
public:
    /**
     * @brief The answer of the asynchronous `develop` check for ONE requirement.
     *
     * `paths` is what the answer is about: an answer to a different path list is
     * not applied (`applyCodeAnswer()`), because the file changed between the
     * question and the answer.
     */
    struct CodeAnswer
    {
        QStringList paths;          //!< the cited paths this answer was computed for
        bool answered = false;      //!< false = the check has not finished yet
        bool inDevelop = false;     //!< every cited path resolves in some `develop`
        QStringList missingPaths;   //!< asked and not found in any `develop`
        QString branch;             //!< the branch that carries them instead
        QString error;              //!< why the check could not run at all
    };

    /**
     * @brief Fills the file half of the DONE gate (no process, no git).
     *
     * Sets `allCriteriaDone`, the review record and its verdict, the zone flag —
     * and leaves the code half **pending** when the requirement cites a path.
     * Run once per requirement per reload, before `DoneGateCheck::start()`.
     *
     * @param req Requirement to evaluate (fields updated in place).
     * @param repoRoots Loaded requirement roots; each is a candidate repo for
     *        `plans/reviews/`.
     */
    static void evaluateLocal(Requirement &req, const QVector<RequirementRoot> &repoRoots);

    /**
     * @brief Records the `git` answer of `DoneGateCheck` in @p req.
     *
     * An unanswered or stale answer leaves the code half pending; it never
     * passes. A requirement that cites nothing is resolved by having nothing to
     * verify.
     *
     * @param req Requirement whose local half was already evaluated.
     * @param answer The answer for this requirement (matched by path list).
     */
    static void applyCodeAnswer(Requirement &req, const CodeAnswer &answer);

    /**
     * @brief The answer a previously checked requirement carries, for a re-read.
     *
     * The action re-reads the file half from disk (a review record may be new)
     * and applies this to it: the `git` half is not asked again, because asking
     * it on the caller's thread is the defect this split removes.
     *
     * @param req An evaluated requirement.
     */
    static CodeAnswer codeAnswerOf(const Requirement &req);

    /**
     * @brief The code paths the requirement cites (delegates to the validator).
     *
     * @param req Any requirement; `rawContent` is what is read.
     */
    static QStringList citedPaths(const Requirement &req);

    /**
     * @brief One line per missing precondition; empty when nothing is missing.
     *
     * Each line names the thing and where to fix it: `AC2 не е отметнат`,
     * `няма ревю запис`, `последната присъда е „ВЪРНАТО“`,
     * `кодът още не е в develop (на клона X е): <път>`, `DONE извън archive/`.
     * When the develop check could not run at all the line is
     * `не мога да проверя дали кодът е в develop: <причина>`, and while it is
     * still running the line says so — a refusal, not a pass.
     *
     * @param req An evaluated requirement.
     * @return The list, in a stable order (criteria, review, code, zone).
     */
    static QStringList missingPreconditions(const Requirement &req);

    /**
     * @brief The single decision: IMPLEMENTED and every precondition holds.
     * @param req An evaluated requirement.
     */
    static bool isDoneCandidate(const Requirement &req);

    /**
     * @brief The review record that names @p id, resolved by content.
     *
     * Walks `<root>/DevelopmentProcess/plans/reviews/*.md` (README.md is the
     * template, not a record) and answers with the first file whose text names
     * the ID — a renamed record still answers, a missing one is an empty string
     * and therefore `acceptingReview == false`, never a read error.
     *
     * @param repoRoots Roots to search.
     * @param id Requirement ID (case-insensitive).
     * @return Absolute path, empty when no record names the requirement.
     */
    static QString findReviewRecord(const QVector<RequirementRoot> &repoRoots,
                                    const QString &id);

    /**
     * @brief The verdict of the LAST data row of the `## Кръгове` table.
     *
     * The table of `reviews/README.md`: `| итерация | A | отворени | затворени |
     * присъда |`; the last column of the last row is the verdict. A section with
     * no row at all answers with an empty string — "a file exists" is not a
     * review, and treating it as one would drop the gate.
     *
     * @param content Raw review record.
     * @return `ПРИЕТО`, `ПРИЕТО С БЕЛЕЖКИ`, `ВЪРНАТО`, `ESCALATED …`, or "".
     */
    static QString lastReviewVerdict(const QString &content);

    /**
     * @brief Whether @p verdict lets the requirement through the gate.
     * @param verdict The token lastReviewVerdict() returned.
     */
    static bool isAcceptingVerdict(const QString &verdict);

private:
    //!< The blocks missingPreconditions() concatenates, one per precondition.
    static QStringList criteriaGaps(const Requirement &req);
    static QString reviewGapFor(const Requirement &req);
    static QString codeGapFor(const Requirement &req);
    static QString zoneGapFor(const Requirement &req);
};

} // namespace Daqster
