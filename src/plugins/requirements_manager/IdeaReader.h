#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Daqster {

/**
 * @brief One idea record read from `<root>/DevelopmentProcess/plans/ideas/*.md`.
 *
 * An idea is a NOTE, not a requirement: it never enters the Requirement
 * collection and never reaches RequirementsValidator or the traceability
 * matrix. Keeping it in its own type is what keeps the validator's scope equal
 * to what RequirementsParser walked (REQ-SW-PL-008).
 *
 * Recognised header (tolerant):
 *   > Статус: в развитие · Добавена: 2026-10-17 · Следващ преглед: 2027-01-17
 *   > **Статус:** узряла | **Добавена:** ... | **Следващ преглед:** ...
 * Fields may be separated by `·` or `|`, in any order, and the key may be
 * wrapped in `**` (bold) — see IdeaHeaderKeys for the configurable names.
 */
struct Idea
{
    QString id;                 //!< "IDEA-001" (from the file name, title or header)
    QString title;              //!< first `# ` heading, ID prefix stripped
    QString status;             //!< e.g. "в развитие"; empty when absent
    QString added;              //!< ISO date from "Добавена:"; empty when absent
    QString nextReview;         //!< ISO date from "Следващ преглед:"; empty when absent
    QString promotedId;         //!< REQ the idea became ("Промотирана:" / "Изискване:");
                                //!< the FIRST of the two keys in the file; empty when the
                                //!< idea is not promoted (REQ-SW-PL-098)
    //!< The two keys are kept apart as well, because they can disagree: taking
    //!< only the first would silently lose the other, so
    //!< RequirementsValidator::validateIdeaLinks() turns the conflict into an
    //!< Error instead of a winner (REQ-SW-PL-098 review R1-3).
    QString promotedKeyId;      //!< value of "Промотирана:"; empty when absent
    QString requirementKeyId;   //!< value of "Изискване:"; empty when absent
    QString path;               //!< absolute path of the idea .md
    QString rawContent;         //!< the whole file, for the read-only preview
    int attachmentCount = 0;    //!< files under `<ideas>/<id>/attachments/`

    bool isValid() const { return !id.isEmpty() && !title.isEmpty(); }
};

/**
 * @brief Names of the header fields an idea file may carry.
 *
 * The conventional defaults are what `.../plans/ideas/README.md` prescribes;
 * a test (or a future scene) can pass a different set. The struct exists so the
 * reader never hard-codes the Bulgarian labels in its matching logic.
 */
struct IdeaHeaderKeys
{
    QString status = QStringLiteral("Статус");
    QString added = QStringLiteral("Добавена");
    QString nextReview = QStringLiteral("Следващ преглед");
    QString promoted = QStringLiteral("Промотирана");
    //!< The same link under the register's other name: IDEA-002 carries
    //!< `> Изискване: REQ-AI-028` (REQ-SW-PL-098). One link, one home — both
    //!< keys feed Idea::promotedId and nothing reads the requirement's file.
    QString requirement = QStringLiteral("Изискване");
};

/**
 * @brief Reads the idea register (read-only) without touching requirements.
 *
 * @par Scope
 * Only `*.md` files directly inside a candidate ideas directory are read; the
 * `<IDEA>/attachments/` subdirectory is counted, never parsed. Both
 * `<root>/DevelopmentProcess/plans/ideas/` (current convention) and
 * `<root>/DevelopmentProcess/ideas/` (older layout) are candidates, derived
 * from the roots the manager already opened — no new settings mechanism.
 *
 * @par Robustness
 * Never throws. A file whose name/title carries an IDEA id but that has no
 * title heading is skipped with one `qCWarning` naming the file; a file that is
 * not an idea at all (the register's README, a template) is skipped with one
 * `qCDebug` line.
 */
class IdeaReader
{
public:
    /**
     * @brief The ideas directories that exist under @p roots, deduplicated.
     *
     * @param roots Repo roots as opened by RequirementsWidget (also accepts a
     *        path that is itself an ideas directory).
     * @return Absolute, canonical directory paths; empty when none exists.
     */
    static QStringList candidateDirs(const QStringList &roots);

    /**
     * @brief Reads every idea file reachable from @p roots, sorted by ID.
     *
     * Duplicate files reachable through overlapping roots are read once (keyed
     * by canonical file path). Files are not re-read between the roots.
     */
    static QVector<Idea> readFromRoots(const QStringList &roots,
                                       const IdeaHeaderKeys &keys = IdeaHeaderKeys());

    /**
     * @brief Reads the `*.md` files of one ideas directory.
     *
     * @param ideasDir Directory to walk (missing/unreadable → empty result).
     * @param keys Header field names to recognise.
     * @return Parsed ideas in file-name order (caller may sort).
     */
    static QVector<Idea> readDirectory(const QString &ideasDir,
                                       const IdeaHeaderKeys &keys = IdeaHeaderKeys());

    /**
     * @brief Parses a single idea file.
     *
     * @param filePath The `.md` file.
     * @param keys Header field names to recognise.
     * @param ok Set to false when the file is not a readable idea record.
     * @return The parsed idea; only meaningful when @p ok is true.
     */
    static Idea parseFile(const QString &filePath, const IdeaHeaderKeys &keys,
                          bool *ok);

    /**
     * @brief Appends `> **Промотирана:** REQ-…` to the idea's header block.
     *
     * The line is written as a separate header line; every other byte of the
     * file is preserved. When a promotion line already exists it is replaced
     * instead of duplicated.
     *
     * @param ideaPath The idea `.md` to update.
     * @param requirementId The requirement the idea became.
     * @return true when the file was written.
     */
    static bool appendPromotionLine(const QString &ideaPath,
                                    const QString &requirementId);

    /**
     * @brief Directory holding an idea's attachments, or empty when absent.
     */
    static QString attachmentsDir(const QString &ideaPath, const QString &ideaId);
};

} // namespace Daqster

Q_DECLARE_METATYPE(Daqster::Idea)
