#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QFileInfo>
#include <QMetaType>

namespace Daqster {

// Subdirectory (relative to a repo root) that contains the requirements tree.
// Formerly the top-level requirements/ dir; moved under DevelopmentProcess/ so
// all process knowledge lives in one tracked, tool-agnostic directory.
inline constexpr char kRequirementsSubdir[] = "DevelopmentProcess/requirements";

/**
 * @brief Template generations of a requirement file (REQ-SW-PL-008).
 *
 * One canonical template — the one RDD-PROCESS.md prints — and the two older
 * generations still present in the tree. The single source of both the parser
 * (which records the generation) and the validator (which reports a
 * deviation), so neither can drift from the other or from the process doc.
 *
 * | variant | description heading | status |
 * |---------|--------------------|--------|
 * | v2      | `## Накратко`      | canonical |
 * | v1      | `## Описание`      | recognised, not canonical |
 * | v0      | `## Проблем` (+ `## Цел`) | recognised, not canonical |
 * | unknown | (none)             | no template — an error |
 */
namespace RequirementTemplate {

/** @brief The canonical generation token ("v2"). */
QString canonicalVersion();

/** @brief The canonical description heading, without "## " ("Накратко"). */
QString canonicalHeading();

/**
 * @brief Template generation of a description heading.
 *
 * @param heading A "## " heading's text, prefix already stripped.
 * @return "v2", "v1" or "v0"; an empty string when @p heading is not a
 *         description heading at all.
 */
QString versionForHeading(const QString &heading);

/**
 * @brief The description heading a generation prints, for messages.
 *
 * @param version "v2", "v1", "v0" or anything else.
 * @return The heading ("Накратко", "Описание", "Проблем"), or an empty string
 *         for an unrecognised/absent generation.
 */
QString headingForVersion(const QString &version);

} // namespace RequirementTemplate

/**
 * @brief Structured representation of a single Markdown requirement file.
 *
 * Fields are parsed from the conventional requirement template:
 *   # REQ-XXX: <Title>
 *   - **Статус:** ...
 *   - **Приоритет:** ...
 *   - **Отговорник (роля):** ...
 *   - **Дата:** YYYY-MM-DD
 *   - **Родител:** REQ-XXX
 *   - **Зависи от:** REQ-XXX, REQ-YYY
 *   - **Деца:** REQ-ZZZ
 *   ## Описание | ## Накратко | ## Проблем   (description section, any of the
 *                                          three template generations; the one
 *                                          used is recorded in
 *                                          Requirement::templateVersion)
 *   ## Acceptance Criteria | ## Как ще разберем, че работи | ## Критерии за приемане
 *   - [ ] criterion
 *   - [x] done criterion
 *   ### [x] AC1 — heading criterion     (state in the checkbox)
 *   ### AC2 — heading criterion         (no checkbox → open)
 *   1. numbered criterion               (inside the criteria section)
 *   | 3 | table criterion | ✅ |         (inside the criteria section)
 *   ## Проследимост
 *   - **Коммити:** ...
 *
 * @par Reference grammar (Родител / Зависи от / Деца)
 * A reference is an ID token — `REQ-<PREFIX>-<NNN>` or `REQ-SW-<TYPE>-<NNN>` —
 * optionally followed by a parenthesised annotation, e.g.
 * `REQ-AI-005 (интероп слой — предшественик, \`DONE\`)`. The value is split on
 * TOP-LEVEL commas only: a comma inside the annotation belongs to the
 * annotation. The prose around an ID is never resolved as a requirement, which
 * is what keeps a sentence from being reported as a dangling reference; a
 * non-requirement document ID (`ADR-AI-013`) is kept and reported as such.
 */
struct Requirement
{
    QString id;                    //!< e.g. "REQ-SW-PL-001" (typed) or "REQ-PLG-001"
    QString title;                 //!< e.g. "Requirements Viewer/Editor Tool"
    QString status;                //!< ACTIVE | DONE | CANCELLED
    QString priority;              //!< High | Medium | Low
    QString assignee;              //!< PM | Architect | Implementation | QA
    QString date;                  //!< creation/modification date (ISO), from "- **Дата:**"
    QString parentId;              //!< parent requirement ID, empty when none ("—")
    QStringList parentIds;         //!< every ID in "Родител:" (parentId is the first
                                   //!< requirement one); a requirement may list several
    QStringList dependencies;      //!< requirement IDs this depends on ("Зависи от:")
    QStringList childIds;          //!< IDs listed in "Деца:" (inverse of "Родител:")
    QString description;           //!< free text under "## Описание"
    QString traceability;          //!< free text under "## Проследимост"
    QString commits;               //!< "- **Коммити:**" under Проследимост (commit hash list)
    QString code;                  //!< "- **Код:**" under Проследимост (code location)
    QString tests;                 //!< "- **Тестове:**" under Проследимост (test description)
    QString docs;                  //!< "- **Документация:**" under Проследимост (doc location)
    QStringList acceptanceCriteria; //!< criterion text (without [ ] / [x] marker)
    QVector<bool> criteriaDone;    //!< parallel to acceptanceCriteria
    QString filePath;              //!< absolute path of the .md file
    QString fileName;              //!< base name, e.g. "REQ-SW-PL-001-....md"
    QString section;               //!< "active" | "archive"
    QString repo;                  //!< "public" | "private" | "other" (derived from the ID
                                   //!< prefix in parseDirectories; empty when parsed via
                                   //!< parseDirectory alone, keeping single-root callers
                                   //!< compile- and behavior-compatible)
    int revision = 0;              //!< "- **Ревизия:** N" (REQ-SW-PL-101);
                                   //!< 0 when the field is absent
    // ── Ревизия, одобрение и базова линия (REQ-SW-PL-106) ─────────────────
    // The approval is pinned to a NUMBER, not to "approved at some point":
    // "Одобрено: 2026-11-22, потребител (ревизия 2)" says WHICH contract the
    // human accepted. A line without the number is LEGACY (143 files predate
    // the format) and is read as "approved up to the current revision" —
    // normalised in the parser, so no file has to be rewritten.
    int approvedRevision = -1;     //!< N from "(ревизия N)"; -1 = no approval
                                   //!< line; normalised to `revision` when the
                                   //!< line carries no number (legacy)
    QString approvedDate;          //!< date part of "- **Одобрено:**"
    QString approvedBy;            //!< who part ("потребител"); empty when absent
    QString baselineDate;          //!< "- **Базова линия:**" date — the first
                                   //!< commit that carried code for this REQ
    QString baselineRef;           //!< branch/ref on the baseline line
    int baselineRevision = -1;     //!< N from "(ревизия N)" on that line
    QString created;               //!< "- **Създадено:**" — the immutable creation
                                   //!< date (REQ-SW-PL-101). A file written before
                                   //!< that field existed carries it in "- **Дата:**",
                                   //!< which stays a VALID field: the parser aliases
                                   //!< it here instead of migrating 135 files. Use
                                   //!< createdDate() rather than reading either field.
    QString rawContent;            //!< original file content (for round-trip edit)
    QString templateVersion;       //!< template generation the file was written
                                   //!< with, by its description heading:
                                   //!< "v2" (## Накратко, canonical), "v1"
                                   //!< (## Описание), "v0" (## Проблем/## Цел) or
                                   //!< "unknown" (no description heading at all).
                                   //!< Empty only for records a caller built by
                                   //!< hand (no .md behind them) — the validator
                                   //!< grades parsed files only.
    QString templateHeading;       //!< the description heading actually found,
                                   //!< without the "## " prefix (empty when none)

    /**
     * @brief Cross-repo annotations preserved from "Родител:" / "Зависи от:".
     *
     * Keys are BARE IDs (annotation stripped for resolution); values are the
     * raw annotation text (e.g. "публично", "частно"). Only entries that
     * carried an annotation are present. Same key scheme for parents and
     * dependencies so the validator can check the implied repo against the
     * resolved requirement's repo.
     */
    QHash<QString, QString> dependencyHints;

    // ── Порта към DONE (REQ-SW-PL-107): ИЗЧИСЛЕНИ полета ──────────────────
    // None of these is a line in the .md file and none of them is a status: the
    // file keeps exactly the fields it had. allCriteriaDone comes from the
    // parse; the review fact is filled by DoneGate::evaluateLocal() and the
    // develop fact by DoneGate::applyCodeAnswer() from the asynchronous
    // DoneGateCheck; doneCandidate is the single decision over them, so the tree
    // badge and the action cannot disagree.
    bool allCriteriaDone = false;   //!< every AC is `[x]` (and there is at least one)
    QString reviewRecord;           //!< path of the review record naming this ID
    QString reviewVerdict;          //!< its last `## Кръгове` verdict
    bool acceptingReview = false;   //!< record exists and its verdict accepts
    bool codeInDevelop = false;     //!< every cited path resolves in `develop`
    QStringList missingDevelopPaths; //!< cited paths absent from `develop`
    QString codeBranch;             //!< branch that carries them instead
    QString codeCheckError;         //!< why the `develop` check could not run at
                                    //!< all (no repo, no branch, no git); empty
                                    //!< when it ran — a refusal, never a pass
    bool codeCheckPending = false;  //!< the asynchronous check has not answered
                                    //!< yet; pending is a refusal, never a pass
    bool doneCandidate = false;     //!< IMPLEMENTED + the four preconditions

    // ── Произход (REQ-SW-PL-098): ИЗЧИСЛЕНО поле ──────────────────────────
    // The idea this requirement grew out of. Not a line of the .md file: the
    // link has ONE home — the idea's own first line ("Промотирана:" /
    // "Изискване:") — so the widget fills this once per load from the idea
    // register and nothing writes it back (REQ-SW-PL-098 → Технически детайли).
    QString originIdeaId;           //!< "IDEA-002", or empty when no idea names it
};

/**
 * @brief Phase progression indicators for a requirement (REQ-SW-PL-017).
 *
 * Derived from the traceability fields: architecture ← "Документация:",
 * implementation ← "Код:", testing ← "Тестове:". A non-empty field after
 * trim means "записано" (recorded); empty, whitespace or "—" means "not
 * recorded". The flags are INFORMATIVE only — a true flag means the phase
 * artifact is recorded, NOT that it is verified/working. Status remains the
 * coarse gate (DONE requires full verification per RDD-PROCESS).
 */
struct PhaseStatus
{
    bool architecture = false; //!< "Документация:" recorded
    bool implementation = false; //!< "Код:" recorded
    bool testing = false;      //!< "Тестове:" recorded
};

/**
 * @brief Computes the phase status indicators for a requirement (QtCore-only,
 *        unit-testable in the headless requirements_manager_tests binary).
 * @param req The parsed requirement.
 * @return PhaseStatus with per-phase "recorded" flags.
 */
PhaseStatus phaseStatus(const Requirement& req);

/**
 * @brief The requirement's creation date (REQ-SW-PL-101).
 *
 * `- **Създадено:**` when present, the legacy `- **Дата:**` otherwise. One
 * accessor for the whole tree, so the model, the preview, the validator and
 * the waiting counter cannot disagree about which field is "created". A
 * record built by hand (a test fixture) that only fills `date` is handled the
 * same way as a file written before the field existed.
 *
 * @param req The parsed requirement.
 * @return ISO date, or an empty string when neither field is present.
 */
QString createdDate(const Requirement& req);

/**
 * @brief One requirements tree root (a repo root containing
 *        DevelopmentProcess/requirements) for the merged multi-repo parse.
 */
struct RequirementRoot
{
    QString repoRoot; //!< absolute path of a repo root directory
};

/**
 * @brief Parses requirement Markdown files from a base requirements directory.
 *
 * Expected layout:
 *   <baseDir>/DevelopmentProcess/requirements/README.md
 *   <baseDir>/DevelopmentProcess/requirements/traceability-matrix.md
 *   <baseDir>/DevelopmentProcess/requirements/active/*.md
 *   <baseDir>/DevelopmentProcess/requirements/archive/*.md
 */
class RequirementsParser
{
public:
    /**
     * @brief Parses one requirements tree and returns its requirements.
     *
     * @par Scope
     * @p baseDir may be a repo root or the requirements directory itself; in
     * both cases the walk is limited to the requirements tree
     * (DevelopmentProcess/requirements — active, archive and nested
     * subdirectories). The rest of the repo is never parsed, which matches the
     * scope of scripts/validate-traceability.sh: a draft under
     * DevelopmentProcess/plans/ cannot become a "requirement" the CLI does not
     * know about.
     *
     * @param baseDir Repo root or requirements directory to parse.
     * @return Requirements sorted by ID; files without a parser-conformant H1
     *         are skipped.
     */
    static QVector<Requirement> parseDirectory(const QString &baseDir);

    /**
     * @brief Parses several requirements tree roots and merges them into one
     *        vector (REQ-SW-PL-012).
     *
     * Each root is parsed with parseDirectory(); every resulting requirement
     * is stamped with @c repo derived from its ID prefix (REQ-SW-* -> "public",
     * other REQ-* -> "private", anything else -> "other"). The merged vector
     * is stable-sorted by (id, repo) case-insensitively.
     */
    static QVector<Requirement> parseDirectories(const QVector<RequirementRoot> &roots);

    /**
     * @brief Discovers the requirements tree roots to load (REQ-SW-PL-012).
     *
     * Walks up from the application binary directory to find the primary root
     * (the first directory containing DevelopmentProcess/requirements — the
     * historical single-root behaviour), then scans the parent directory for
     * SIBLING directories that also contain DevelopmentProcess/requirements.
     * Returns canonical absolute paths, deduplicated and sorted.
     */
    static QStringList discoverRepoRoots();

    /**
     * @brief Derives the repo label from a requirement ID prefix.
     *        "REQ-SW-*" -> "public", any other "REQ-*" -> "private",
     *        anything else -> "other".
     */
    static QString repoForId(const QString &id);

    /**
     * @brief Persists a requirement back to its .md file.
     * @return true on success, false on failure.
     */
    static bool writeRequirement(const Requirement &req);

    /**
     * @brief Updates the checkbox state of a single acceptance criterion
     *        inside req.rawContent (in-place line replacement).
     */
    static void setCriterionChecked(Requirement &req, int index, bool done);

    /**
     * @brief Updates the "- **Статус:**" metadata line inside req.rawContent.
     */
    static void setStatusLine(Requirement &req, const QString &status);

    /**
     * @brief Rewrites the "- **Зависи от:**" metadata line inside req.rawContent.
     *        If no such line exists it is inserted before the first "## " heading.
     */
    static void setDependenciesLine(Requirement &req, const QStringList &dependencies);

    /**
     * @brief Approves a DRAFT requirement file (REQ-SW-PL-099).
     *
     * Rewrites `- **Статус:** DRAFT` into `- **Статус:** ACTIVE` and puts
     * `- **Одобрено:** <date>, потребител (ревизия N)` on the line directly
     * after it (replacing an existing approval line rather than adding a second
     * one). N is the revision the file carries after the history row this call
     * appends, i.e. the file the human accepted (REQ-SW-PL-106) — reading it
     * back must not leave `Ревизия > одобрена`, which would block the very
     * requirement just approved.
     * Every other byte of the file is left as it is; the write goes through
     * QSaveFile, so a failure leaves the original in place.
     *
     * A file whose status is not DRAFT is not a draft any more: the call is a
     * successful no-op and writes nothing, so a second accept cannot turn
     * ACTIVE into ACTIVE with a new date.
     *
     * @param filePath Path to the requirement .md file.
     * @param date ISO date of the approval (YYYY-MM-DD).
     * @param errorOut Optional human-readable reason when the call returns false.
     * @return true when the file is now ACTIVE (rewritten, or already not DRAFT).
     */
    static bool approveDraft(const QString &filePath, const QString &date,
                             QString *errorOut = nullptr);

    /**
     * @brief The heading and column row of the draft review table
     *        (REQ-SW-PL-102).
     *
     * One spelling for the file, the model, the widget and
     * `scripts/check-req-approval.sh`: the script greps the same heading, so
     * a rename here without the script is a defect, not a style choice.
     */
    static QString draftReviewHeading();

    /** @brief The verdict tokens the table carries (lower-case, Bulgarian). */
    static QString changesRequestedVerdict();
    static QString rejectVerdict();

    /**
     * @brief Number of review rounds recorded in @p content (REQ-SW-PL-102).
     *
     * Counts the DATA rows of the `## Ревю на черновата` table: the header row
     * and the `|---|` separator do not count. 0 means "nobody has judged this
     * draft yet" — which is what the widget's "чакат теб" counter means.
     *
     * @param content Raw file content (or any text carrying the table).
     * @return Row count, 0 when the section or the table is absent.
     */
    static int draftReviewRounds(const QString &content);

    /**
     * @brief The verdict of the LAST row of the review table.
     *
     * @param content Raw file content.
     * @return `за корекция`, `откажи`, or an empty string when no verdict was
     *         recorded at all.
     */
    static QString lastDraftVerdict(const QString &content);

    /**
     * @brief "За корекция": records a review round, status stays DRAFT.
     *
     * Appends ONE row to `## Ревю на черновата` (`| N | date | за корекция |
     * note |`) and writes nothing else — the status line is not touched, which
     * is what the acceptance criterion checks with `git diff`.
     *
     * A file whose status is not DRAFT is a successful no-op (the same model
     * approveDraft() follows): the action only exists for a draft.
     *
     * @param filePath Path to the requirement .md file.
     * @param note What is missing; empty means "no review happened" and fails.
     * @param date ISO date of the verdict (YYYY-MM-DD).
     * @param errorOut Optional human-readable reason on failure.
     * @return true when the verdict was recorded (or the file was not a draft).
     */
    static bool requestChangesDraft(const QString &filePath, const QString &note,
                                    const QString &date, QString *errorOut = nullptr);

    /**
     * @brief "Откажи": the draft is cancelled, moved to archive/.
     *
     * Writes `- **Статус:** CANCELLED`, the review row and a history row (the
     * status changed, so `scripts/check-req-history.sh` demands one), then
     * moves the file from active/ to archive/ with a STRICT rename: an
     * existing file of the same name is an error, never an overwrite —
     * "нищо не се трие" (REQ-SW-PL-102).
     *
     * A non-DRAFT file is a successful no-op.
     *
     * @param filePath Path to the requirement .md file in active/.
     * @param reason Why it is refused; empty fails.
     * @param date ISO date of the verdict.
     * @param errorOut Optional human-readable reason on failure.
     * @return true when the file is CANCELLED and archived.
     */
    static bool rejectDraft(const QString &filePath, const QString &reason,
                            const QString &date, QString *errorOut = nullptr);

    /**
     * @brief "Промени договора" (REQ-SW-PL-101): a contract change goes back to
     *        DRAFT and needs a fresh approval.
     *
     * Rewrites `- **Статус:**` to `DRAFT`, appends a history row naming the
     * change and the reason, and bumps `- **Ревизия:**`. Only an ACTIVE or
     * IMPLEMENTED requirement has a contract to change; anything else is a
     * successful no-op.
     *
     * The approval line is SEALED, not deleted (REQ-SW-PL-106): an
     * `- **Одобрено:**` without a number gets `(ревизия N)` for the revision
     * that was accepted, so the file reads "revision N accepted, N+1 is the
     * change nobody judged" instead of pretending nothing was approved.
     *
     * @param filePath Path to the requirement .md file.
     * @param reason What is being added/removed/changed; empty fails.
     * @param who Who asked for the change (written into the row).
     * @param date ISO date of the change.
     * @param errorOut Optional human-readable reason on failure.
     * @return true when the file is DRAFT again (or was not ACTIVE/IMPLEMENTED).
     */
    static bool startContractChange(const QString &filePath, const QString &reason,
                                   const QString &who, const QString &date,
                                   QString *errorOut = nullptr);

    /**
     * @brief Appends a history row to @p req and bumps its revision in place
     *        (REQ-SW-PL-101).
     *
     * The row is `| date | change | who | why |` under `## История`, which is
     * created at the end of the file when absent. The revision becomes
     * `max(revision, historyRows)`, so a hand-written file without the field
     * starts counting from the rows it already carries.
     *
     * @param req Requirement whose rawContent is edited (and revision updated).
     * @param date ISO date of the change.
     * @param change What changed (one short clause).
     * @param who Who changed it.
     * @param why Why (the reason a reader needs a month later).
     */
    static void appendHistoryRow(Requirement &req, const QString &date,
                                 const QString &change, const QString &who,
                                 const QString &why);

    /**
     * @brief The revision number written in @p content ("- **Ревизия:** N").
     * @return N, or 0 when the field is absent.
     */
    static int revisionInContent(const QString &content);

    /**
     * @brief „Mark Done & Archive“: the owner's `DONE` verdict (REQ-SW-PL-107).
     *
     * Writes `- **Статус:** DONE`, EXACTLY one history row
     * (`| <date> | DONE | потребител | ревю <файл> |`), bumps `- **Ревизия:**`,
     * and moves the file to archive/ with a STRICT rename — an existing file of
     * the same name is an error, never an overwrite.
     *
     * The preconditions (every AC checked, an accepting review record, the code
     * in `develop`) are checked by DoneGate before this is called: the
     * transition itself does not own them, so one function is the authority
     * (see DoneGate::missingPreconditions()).
     *
     * A file that is already DONE is a successful no-op.
     *
     * @param filePath Path to the requirement .md file in active/.
     * @param reviewFile The review record that accepts it (for the history row).
     * @param date ISO date of the verdict.
     * @param errorOut Optional human-readable reason on failure.
     * @return true when the file is DONE and archived.
     */
    static bool markDone(const QString &filePath, const QString &reviewFile,
                         const QString &date, QString *errorOut = nullptr);

    /**
     * @brief Resolves the file on disk by the ID in its H1 (REQ-SW-PL-102 AC7).
     *
     * The review actions must never act on a cached path: a file renamed or
     * moved by git/hand keeps its H1 and its meaning, while a cached path
     * points at nothing. This re-walks the tree and answers with the file whose
     * parsed ID matches — or an empty string, which the caller reports as
     * "преименуван или преместен".
     *
     * @param roots Requirement tree roots to walk.
     * @param id Requirement ID to find (case-insensitive).
     * @return Absolute path, empty when no file carries that H1.
     */
    static QString findFileById(const QVector<RequirementRoot> &roots, const QString &id);

    /**
     * @brief Scans all parsed requirements in active/ and archive/ under baseDir
     *        for the given prefix and returns the next free ID as "REQ-<P>-<NNN>"
     *        (zero-padded to 3 digits). E.g. generateNextId(dir, "SW-PL") -> "REQ-SW-PL-016".
     */
    static QString generateNextId(const QString &baseDir, const QString &prefix);

    /**
     * @brief Moves a requirement .md file from <...>/active/ to <...>/archive/.
     *
     * STRICT on purpose (REQ-SW-PL-107 AC4): an archive file of the same name
     * is an error, never an overwrite — "нищо не се трие", the same behaviour
     * „Откажи“ already has. A requirement that once reached archive/ keeps its
     * bytes.
     *
     * @return true on success, false if the file was not in the active directory
     *         or the rename failed. The target directory is created if needed.
     */
    static bool moveToArchive(const QString &filePath);

    /**
     * @brief Moves a requirement .md file from <...>/archive/ back to <...>/active/.
     * @return true on success, false if the file was not in the archive directory
     *         or the rename failed. The target directory is created if needed.
     */
    static bool moveToActive(const QString &filePath);

    /**
     * @brief Returns the "active" requirements directory for a base directory.
     *        Accepts either the repo root (containing DevelopmentProcess/
     *        requirements) or the DevelopmentProcess/requirements/ directory
     *        itself.
     */
    static QString activeDirectory(const QString &baseDir);

    /**
     * @brief Returns the "archive" requirements directory for a base directory.
     *        Accepts either the repo root (containing DevelopmentProcess/
     *        requirements) or the DevelopmentProcess/requirements/ directory
     *        itself.
     */
    static QString archiveDirectory(const QString &baseDir);

private:
    static bool parseFile(const QFileInfo &fileInfo, const QString &section,
                          Requirement &out);
};

} // namespace Daqster

Q_DECLARE_METATYPE(Daqster::Requirement)
