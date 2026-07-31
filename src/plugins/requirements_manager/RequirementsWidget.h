#pragma once

#include <QWidget>
#include <QVector>
#include "RequirementsParser.h"
#include "RequirementsValidator.h"

class QTreeView;
class QTextBrowser;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class QListWidget;
class QSplitter;
class QComboBox;
class QUrl;

namespace Daqster {

class RequirementsModel;

/**
 * @brief Requirements Viewer/Editor widget.
 *
 * Left: tree of requirements with a view-mode toggle
 *   ("By Section" | "By Hierarchy").
 * Right: details panel with two modes:
 *   - Preview Mode: read-only formatted view with interactive acceptance
 *     criteria checkboxes, clickable relationship links (Родител, Деца,
 *     Зависи от, Зависими от) and lifecycle actions.
 *   - Edit Mode: raw Markdown editor (QPlainTextEdit) with Save/Cancel.
 *
 * Additional actions: create new requirements (NewRequirementDialog),
 * mark done & archive / reopen (REQ-SW-004), dependency editing
 * (REQ-SW-007), validation report (REQ-SW-008) and built-in help
 * (REQ-SW-005).
 *
 * Changes are written back to the .md files (bidirectional sync).
 */
class RequirementsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RequirementsWidget(QWidget *parent = nullptr);
    ~RequirementsWidget() override;

    void openDirectory(const QString &baseDir);

    QString baseDirectory() const;

private slots:
    void onSelectionChanged();
    void onEditToggled(bool edit);
    void onSave();
    void onCancel();
    void onCriterionToggled(int index, bool done);
    void onBrowseDirectory();
    void onNewRequirement();
    void onMarkDoneAndArchive();
    void onReopen();
    void onEditDependencies();
    void onValidate();
    void onShowHelp();
    void onAnchorClicked(const QUrl &link);
    void onViewModeChanged(int index);

private:
    void reload();
    void showPreview();
    void updatePreviewText(const Requirement &req);
    void navigateToId(const QString &id);
    void updateValidationStatus();
    void refreshActionState();
    QString linkFor(const QString &id) const;

    RequirementsModel *m_model;
    QTreeView *m_treeView;
    QComboBox *m_viewModeCombo;
    QTextBrowser *m_preview;
    QPlainTextEdit *m_editor;
    QLabel *m_fileLabel;
    QLabel *m_validationLabel;
    QPushButton *m_editButton;
    QPushButton *m_saveButton;
    QPushButton *m_cancelButton;
    QPushButton *m_browseButton;
    QPushButton *m_newButton;
    QPushButton *m_doneButton;
    QPushButton *m_reopenButton;
    QPushButton *m_depsButton;
    QPushButton *m_validateButton;
    QPushButton *m_helpButton;
    QListWidget *m_criteriaList;
    QSplitter *m_splitter;

    QString m_baseDir;
    QVector<Requirement> m_requirements;
    QVector<RequirementsValidator::Issue> m_validationIssues;
    int m_currentIndex; //!< index into m_requirements; -1 when none selected
};

} // namespace Daqster
