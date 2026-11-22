#pragma once

#include <QWidget>
#include <QVector>
#include "RequirementsParser.h"
#include "RequirementsValidator.h"
#include "IdeaReader.h"

class QTreeView;
class QTableView;
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
class QFileSystemWatcher;
class QModelIndex;

namespace Daqster {

class RequirementsModel;
class RequirementsSortProxyModel;
class IdeaTableModel;
class IdeaFilterProxyModel;
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
 * The "Идеи" tab (REQ-SW-PL-095) is a read-only view of the idea register
 * derived from the same roots. It has its own model and is structurally
 * outside the requirement collection: nothing there feeds the validator, the
 * matrix or any count. The tab is hidden when no idea files exist.
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
     * @brief Approves the DRAFT requirement @p id and rewrites its file.
     *
     * The transition itself, without the confirmation dialog (that belongs to
     * the action, see onAcceptDraft()): the file becomes ACTIVE with
     * `- **Одобрено:** <date>, потребител` under the status line, and the tree
     * is reloaded. Exposed so the widget glue is driveable from a test.
     *
     * @p id is taken **by value** on purpose: the call ends in reload(), which
     * reassigns m_requirements and frees the elements, so an id that aliased
     * one of them (the action path passes `m_requirements.at(i).id`) would
     * dangle inside navigateToId(). By value the copy is made at the call
     * site, before the reload can invalidate anything.
     *
     * @param id Requirement ID (bare, e.g. "REQ-SW-PL-096"), by value.
     * @param date ISO date of the approval (YYYY-MM-DD).
     * @return true when the file is ACTIVE afterwards.
     */
    bool acceptDraft(QString id, const QString &date);

    /**
     * @brief „За корекция“: records a review round; status stays DRAFT
     *        (REQ-SW-PL-102 AC1).
     *
     * The transition only — the dialog with the non-empty note lives in
     * onRequestChanges(), so the file glue is driveable from a test. The file
     * is resolved BY THE ID IN ITS H1 (AC7), not by a cached path: a renamed
     * file still answers, and a missing one is reported with
     * missingFileMessage() instead of a bare read error.
     *
     * @param id Requirement ID (by value — reload() reallocates the vector).
     * @param note What is missing (required).
     * @param date ISO date of the verdict.
     * @return true when the row was written.
     */
    bool requestChanges(QString id, const QString &note, const QString &date);

    /**
     * @brief „Откажи“: CANCELLED + review row + history row, moved to archive/.
     * @param id Requirement ID (by value).
     * @param reason Why it is refused (required).
     * @param date ISO date of the verdict.
     * @return true when the file is cancelled and archived.
     */
    bool rejectDraft(QString id, const QString &reason, const QString &date);

    /**
     * @brief „Промени договора“: back to DRAFT, history row, revision + 1
     *        (REQ-SW-PL-101 AC4).
     * @param id Requirement ID (by value).
     * @param reason What is being changed (required).
     * @param date ISO date of the change.
     * @return true when the file is DRAFT again.
     */
    bool changeContract(QString id, const QString &reason, const QString &date);

    /**
     * @brief What still blocks `DONE` for @p id, one line per precondition
     *        (REQ-SW-PL-107 AC1).
     *
     * Re-evaluates the requirement against the loaded roots (review record,
     * `develop`, criteria) and returns DoneGate::missingPreconditions(). Empty
     * means the action would go through; the action shows this exact list, so
     * the message and the test cannot drift apart.
     *
     * @param id Requirement ID (by value — the call may reload the tree).
     * @return Missing preconditions, empty when nothing is missing.
     */
    QStringList doneGateMessage(QString id) const;

    /**
     * @brief „Mark Done & Archive“ without the dialog (REQ-SW-PL-107).
     *
     * The transition only: refuses (returns false, writes nothing) while
     * doneGateMessage() is non-empty; otherwise writes DONE, one history row,
     * the revision bump and moves the file to archive/ strictly. Exposed so the
     * refusal and the archive are driveable from a test.
     *
     * @param id Requirement ID (by value).
     * @param date ISO date of the verdict.
     * @return true when the file is DONE and in archive/ afterwards.
     */
    bool markDone(QString id, const QString &date);

    /**
     * @brief The message a review action shows when the file cannot be found.
     *
     * A file renamed or moved by git/hand keeps its H1 and its meaning, while a
     * cached path stops existing (REQ-SW-PL-102 AC7). Exposed so the test can
     * assert the sentence instead of a modal box's implementation.
     *
     * @param id The requirement ID that could not be resolved.
     * @return The full sentence, naming the requirement and the remedy.
     */
    static QString missingFileMessage(const QString &id);

    /**
     * @brief Resolves @p id to the file whose H1 carries it, on disk now.
     *
     * A fresh walk of every loaded root (REQ-SW-PL-102 AC7): the identity of a
     * requirement is its H1, the path is not.
     *
     * @param id Requirement ID (case-insensitive).
     * @return Absolute path, empty when no file carries that ID.
     */
    QString resolveRequirementFile(const QString &id) const;

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
    void onAcceptDraft();
    void onRequestChanges();
    void onRejectDraft();
    void onChangeContract();
    void onWatchedPathChanged();
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
    void onIdeaSelectionChanged();
    void onIdeaSearchChanged(const QString &text);
    //!< Click on the "Изискване" cell opens that requirement (REQ-SW-PL-098).
    void onIdeaTableClicked(const QModelIndex &index);
    void onPromoteIdea();

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
    //!< Watches every requirements directory so an external change (git pull,
    //!< rename) refreshes the list (REQ-SW-PL-102 AC7).
    void rebuildWatchedPaths();
    //!< Reports a requirement whose file no longer exists by its H1 (AC7).
    void missingFile(const QString &id);
    //!< Unfinished "Зависи от:" targets for the file (kept in the gate's list).
    QStringList dependencyBlockers(const QString &filePath) const;
    //!< Builds the read-only "Идеи" tab (REQ-SW-PL-095).
    QWidget *createIdeasPanel();
    //!< Re-reads the idea register and hides the tab when it is empty.
    void refreshIdeasTab();
    //!< Renders the selected idea's Markdown through RequirementPreview.
    void showIdeaPreview(const Idea &idea);
    //!< Repo root an idea belongs to (for the promoted requirement's file).
    QString rootForIdea(const Idea &idea) const;
    //!< Fills Requirement::originIdeaId from the idea register once per load
    //!< (REQ-SW-PL-098): one link, read from the idea's own header.
    void linkIdeaOrigins();
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
    QLabel *m_draftCountLabel; //!< "чакат одобрение: N" (REQ-SW-PL-099)
    QLabel *m_doneCandidateLabel; //!< "кандидати за DONE: N" (REQ-SW-PL-107)
    QToolBar *m_actionBar;
    QAction *m_editAction;
    QAction *m_saveAction;
    QAction *m_cancelAction;
    QAction *m_browseAction;
    QAction *m_newAction;
    QAction *m_doneAction;
    QAction *m_reopenAction;
    QAction *m_acceptAction;
    QAction *m_requestChangesAction; //!< „За корекция“ (REQ-SW-PL-102)
    QAction *m_rejectAction;         //!< „Откажи“
    QAction *m_contractAction;       //!< „Промени договора“ (REQ-SW-PL-101)
    QAction *m_depsAction;
    QAction *m_validateAction;
    QAction *m_helpAction;
    QListWidget *m_criteriaList;
    QSplitter *m_splitter;
    QSplitter *m_detailsSplitter; //!< vertical: preview/editor pane ↔ checkboxes
    QTabWidget *m_tabs;
    DependencyGraphWidget *m_graphWidget;
    TraceabilityMatrixWidget *m_matrixWidget;

    // "Идеи" tab (REQ-SW-PL-095) — read-only, structurally outside the
    // requirements collection: nothing here feeds the validator/matrix.
    IdeaTableModel *m_ideaModel;
    IdeaFilterProxyModel *m_ideaProxy;
    QTableView *m_ideasView;
    QLineEdit *m_ideasSearchEdit;
    QTextBrowser *m_ideaPreview;
    QPushButton *m_promoteButton;
    QWidget *m_ideasPanel;
    int m_ideasTabIndex = -1;
    QVector<Idea> m_ideas;
    int m_currentIdeaRow = -1; //!< row in m_ideas; -1 when none selected

    QString m_baseDir;
    QStringList m_roots;                 //!< merged tree roots currently loaded
    QVector<Requirement> m_requirements; //!< FULL parsed set (validation/preview/actions)
    QVector<Requirement> m_filtered;     //!< subset feeding model/graph/matrix
    QVector<RequirementsValidator::Issue> m_validationIssues;
    int m_currentIndex; //!< index into m_requirements; -1 when none selected
    QFileSystemWatcher *m_watcher = nullptr; //!< requirements dirs (AC7)
    QTimer *m_watchTimer = nullptr;          //!< debounce for the watcher
    QAction *m_backAction = nullptr;
    QAction *m_forwardAction = nullptr;
    QVector<QString> m_navHistory;
    int m_navHistoryPos = -1;
};

} // namespace Daqster
