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
 *   ## Описание
 *   ## Acceptance Criteria
 *   - [ ] criterion
 *   - [x] done criterion
 *   ## Проследимост
 *   - **Коммити:** ...
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
    QStringList dependencies;      //!< requirement IDs this depends on ("Зависи от:")
    QString description;           //!< free text under "## Описание"
    QString traceability;          //!< free text under "## Проследимост"
    QString commits;               //!< "- **Коммити:**" under Проследимост (commit hash list)
    QString code;                  //!< "- **Код:**" under Проследимост (code location)
    QString tests;                 //!< "- **Тестове:**" under Проследимост (test description)
    QStringList acceptanceCriteria; //!< criterion text (without [ ] / [x] marker)
    QVector<bool> criteriaDone;    //!< parallel to acceptanceCriteria
    QString filePath;              //!< absolute path of the .md file
    QString fileName;              //!< base name, e.g. "REQ-SW-PL-001-....md"
    QString section;               //!< "active" | "archive"
    QString repo;                  //!< "public" | "private" | "other" (derived from the ID
                                   //!< prefix in parseDirectories; empty when parsed via
                                   //!< parseDirectory alone, keeping single-root callers
                                   //!< compile- and behavior-compatible)
    QString rawContent;            //!< original file content (for round-trip edit)

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
};

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
     * @brief Scans all parsed requirements in active/ and archive/ under baseDir
     *        for the given prefix and returns the next free ID as "REQ-<P>-<NNN>"
     *        (zero-padded to 3 digits). E.g. generateNextId(dir, "SW-PL") -> "REQ-SW-PL-016".
     */
    static QString generateNextId(const QString &baseDir, const QString &prefix);

    /**
     * @brief Moves a requirement .md file from <...>/active/ to <...>/archive/.
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
