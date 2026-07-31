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
 *   ## Описание
 *   ## Acceptance Criteria
 *   - [ ] criterion
 *   - [x] done criterion
 */
struct Requirement
{
    QString id;                    //!< e.g. "REQ-FW-001"
    QString title;                 //!< e.g. "General Requirements Viewer/Editor Tool"
    QString status;                //!< ACTIVE | DONE | CANCELLED
    QString priority;              //!< High | Medium | Low
    QString assignee;              //!< PM | Architect | Implementation | QA
    QString description;           //!< free text under "## Описание"
    QStringList acceptanceCriteria; //!< criterion text (without [ ] / [x] marker)
    QVector<bool> criteriaDone;    //!< parallel to acceptanceCriteria
    QString filePath;              //!< absolute path of the .md file
    QString fileName;              //!< base name, e.g. "REQ-FW-001-....md"
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

private:
    static bool parseFile(const QFileInfo &fileInfo, const QString &section,
                          Requirement &out);
};

} // namespace Daqster

Q_DECLARE_METATYPE(Daqster::Requirement)
