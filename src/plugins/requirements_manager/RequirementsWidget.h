#pragma once

#include <QWidget>
#include <QVector>
#include "RequirementsParser.h"
#include "RequirementsValidator.h"

class QTreeView;
class QTextBrowser;
class QPlainTextEdit;
class QLabel;
class QLineEdit;
class QTimer;
class QPushButton;
class QAction;
class QToolBar;
class QListWidget;
class QSplitter;
class QComboBox;
class QTabWidget;
class QUrl;

namespace Daqster {

class RequirementsModel;
class RequirementsSortProxyModel;
class DependencyGraphWidget;
class TraceabilityMatrixWidget;

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
 * mark done & archive / reopen (REQ-SW-PL-004), dependency editing
 * (REQ-SW-PL-007), validation report (REQ-SW-PL-008) and built-in help
 * (REQ-SW-PL-005). The actions live in a text-only QToolBar, which overflows
 * into an extension menu instead of forcing the window wider than one button
 * (REQ-SW-PL-001).
 *
 * The tree is sorted through RequirementsSortProxyModel: sorting reorders the
 * requirements inside a section and never moves the section rows.
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
    void openDirectories(const QStringList &baseDirs);

    QString baseDirectory() const;

    /**
     * @brief Returns the requirements whose repo matches @p repo
     *        (case-insensitive). An empty/blank @p repo (or "All") returns
     *        the full set. Static so it is directly unit-testable
     *        (REQ-SW-PL-012 shared repo filter).
     */
    static QVector<Requirement> filterRequirementsByRepo(
        const QVector<Requirement> &requirements, const QString &repo);

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
    void onGraphNavigateRequested(const QString &id);
    void onRepoFilterChanged(int index);
    void onSearchTextChanged(const QString &text);
    void onNavBack();
    void onNavForward();

private:
    void reload();
    void showPreview();
    void updatePreviewText(const Requirement &req);
    void navigateToId(const QString &id, bool addToHistory = true);
    void updateValidationStatus();
    void refreshActionState();
    void refreshRepoFilterCombo();
    void applyViewFilters();
    void updateNavButtons();
    void pushNavHistory(const QString &id);
    //!< Requirement behind a *view* index (the tree is sorted by a proxy).
    const Requirement *requirementAtViewIndex(const QModelIndex &viewIndex) const;

    RequirementsModel *m_model;
    RequirementsSortProxyModel *m_sortProxy;
    QTreeView *m_treeView;
    QComboBox *m_viewModeCombo;
    QComboBox *m_repoFilterCombo;
    QLineEdit *m_searchEdit;
    QLabel *m_matchLabel;
    QTimer *m_searchTimer;
    QString m_searchQuery;
    QTextBrowser *m_preview;
    QPlainTextEdit *m_editor;
    QLabel *m_fileLabel;
    QLabel *m_validationLabel;
    QLabel *m_rootStatusLabel;
    QToolBar *m_actionBar;
    QAction *m_editAction;
    QAction *m_saveAction;
    QAction *m_cancelAction;
    QAction *m_browseAction;
    QAction *m_newAction;
    QAction *m_doneAction;
    QAction *m_reopenAction;
    QAction *m_depsAction;
    QAction *m_validateAction;
    QAction *m_helpAction;
    QListWidget *m_criteriaList;
    QSplitter *m_splitter;
    QTabWidget *m_tabs;
    DependencyGraphWidget *m_graphWidget;
    TraceabilityMatrixWidget *m_matrixWidget;

    QString m_baseDir;
    QStringList m_roots;                 //!< merged tree roots currently loaded
    QVector<Requirement> m_requirements; //!< FULL parsed set (validation/preview/actions)
    QVector<Requirement> m_filtered;     //!< subset feeding model/graph/matrix
    QVector<RequirementsValidator::Issue> m_validationIssues;
    int m_currentIndex; //!< index into m_requirements; -1 when none selected
    QAction *m_backAction = nullptr;
    QAction *m_forwardAction = nullptr;
    QVector<QString> m_navHistory;
    int m_navHistoryPos = -1;
};

} // namespace Daqster
