#pragma once

#include <QDialog>
#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QListWidget;
class QPushButton;

namespace Daqster {

/**
 * @brief Dialog for creating a new requirement Markdown file.
 *
 * Generates the next free requirement ID (via RequirementsParser::generateNextId),
 * collects metadata (prefix, title, status, priority, assignee, parent,
 * dependencies), description and acceptance criteria, then writes a
 * fully formatted .md file into the active/ directory.
 */
class NewRequirementDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewRequirementDialog(const QString &baseDir, QWidget *parent = nullptr);

    /**
     * @brief Absolute path of the created .md file, or empty if not created.
     */
    QString createdFilePath() const;

    /**
     * @brief ID of the created requirement, or empty if not created.
     */
    QString createdId() const;

    /**
     * @brief Pre-fills the dialog from an idea (promotion flow).
     *
     * @param title Initial title.
     * @param description Initial description text (the stub quoting the idea).
     * @param fromIdea Idea ID that becomes `- **from_idea:** IDEA-…` in the
     *        created requirement's metadata; empty for a plain requirement.
     */
    void setPrefill(const QString &title, const QString &description,
                    const QString &fromIdea = QString());

private slots:
    void onPrefixChanged();
    void onAddCriterion();
    void onRemoveCriterion();

private:
    void buildMarkdown(QString &outContent, QString &outId, QString &outSlug) const;
    QString activeDirectory() const;
    QString slugFor(const QString &title) const;
    bool writeFile(const QString &content, const QString &id, const QString &slug);

    QString m_baseDir;
    QString m_createdFilePath;
    QString m_createdId;
    QString m_fromIdea; //!< "- **from_idea:**" value; empty for a plain requirement

    QComboBox *m_prefixCombo;
    QLabel *m_idLabel;
    QLineEdit *m_titleEdit;
    QComboBox *m_statusCombo;
    QComboBox *m_priorityCombo;
    QComboBox *m_assigneeCombo;
    QComboBox *m_parentCombo;
    QListWidget *m_dependenciesList;
    QPlainTextEdit *m_descriptionEdit;
    QListWidget *m_criteriaList;
};

} // namespace Daqster
