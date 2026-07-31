#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QFileInfo>
#include <QMetaType>

namespace Daqster {

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
    QString id;                    //!< e.g. "REQ-SW-001"
    QString title;                 //!< e.g. "General Requirements Viewer/Editor Tool"
    QString status;                //!< ACTIVE | DONE | CANCELLED
    QString priority;              //!< High | Medium | Low
    QString assignee;              //!< PM | Architect | Implementation | QA
    QString date;                  //!< creation/modification date (ISO), from "- **Дата:**"
    QString parentId;              //!< parent requirement ID, empty when none ("—")
    QStringList dependencies;      //!< requirement IDs this depends on ("Зависи от:")
    QString description;           //!< free text under "## Описание"
    QString traceability;          //!< free text under "## Проследимост"
    QStringList acceptanceCriteria; //!< criterion text (without [ ] / [x] marker)
    QVector<bool> criteriaDone;    //!< parallel to acceptanceCriteria
    QString filePath;              //!< absolute path of the .md file
    QString fileName;              //!< base name, e.g. "REQ-SW-001-....md"
    QString section;               //!< "active" | "archive"
    QString rawContent;            //!< original file content (for round-trip edit)
};

/**
 * @brief Parses requirement Markdown files from a base requirements directory.
 *
 * Expected layout:
 *   <baseDir>/requirements/README.md
 *   <baseDir>/requirements/traceability-matrix.md
 *   <baseDir>/requirements/active/*.md
 *   <baseDir>/requirements/archive/*.md
 */
class RequirementsParser
{
public:
    static QVector<Requirement> parseDirectory(const QString &baseDir);

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
     *        (zero-padded to 3 digits). E.g. generateNextId(dir, "SW") -> "REQ-SW-012".
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
     *        Accepts either the repo root (containing requirements/) or the
     *        requirements/ directory itself.
     */
    static QString activeDirectory(const QString &baseDir);

    /**
     * @brief Returns the "archive" requirements directory for a base directory.
     *        Accepts either the repo root (containing requirements/) or the
     *        requirements/ directory itself.
     */
    static QString archiveDirectory(const QString &baseDir);

private:
    static bool parseFile(const QFileInfo &fileInfo, const QString &section,
                          Requirement &out);
};

} // namespace Daqster

Q_DECLARE_METATYPE(Daqster::Requirement)
