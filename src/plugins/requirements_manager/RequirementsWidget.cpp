#include "RequirementsWidget.h"

#include "RequirementsModel.h"
#include "RequirementsSortProxyModel.h"
#include "RequirementsParser.h"
#include "RequirementsSearchEngine.h"
#include "IdeaModel.h"
#include "IdeaReader.h"
#include "DependencyGraphWidget.h"
#include "TraceabilityMatrixWidget.h"
#include "NewRequirementDialog.h"
#include "ValidationDialog.h"
#include "HelpDialog.h"
#include "RequirementPreview.h"
#include "RequirementDates.h"
#include "DraftNoteDialog.h"
#include "LogCategories.h"

#include <QTreeView>
#include <QTableView>
#include <QTextBrowser>
#include <QPlainTextEdit>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>
#include <QPushButton>
#include <QAction>
#include <QToolBar>
#include <QListWidget>
#include <QSplitter>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QItemSelectionModel>
#include <QListWidgetItem>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QColor>
#include <QDate>
#include <QDir>
#include <QDirIterator>
#include <QFileSystemWatcher>
#include <QKeySequence>
#include <QHeaderView>

namespace Daqster {

RequirementsWidget::RequirementsWidget(QWidget *parent)
    : QWidget(parent)
    , m_currentIndex(-1)
{
    m_model = new RequirementsModel(this);
    m_sortProxy = new RequirementsSortProxyModel(this);
    m_sortProxy->setSourceModel(m_model);

    m_viewModeCombo = new QComboBox(this);
    m_viewModeCombo->addItem(tr("By Section"));
    m_viewModeCombo->addItem(tr("By Hierarchy"));

    m_repoFilterCombo = new QComboBox(this);
    m_repoFilterCombo->addItem(tr("All repos"), QString());
    m_repoFilterCombo->addItem(QStringLiteral("public"), QStringLiteral("public"));
    m_repoFilterCombo->addItem(QStringLiteral("private"), QStringLiteral("private"));

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setPlaceholderText(tr("Search requirements…"));
    m_matchLabel = new QLabel(this);
    m_matchLabel->setVisible(false);

    m_searchTimer = new QTimer(this);
    m_searchTimer->setSingleShot(true);
    m_searchTimer->setInterval(150);

    m_treeView = new QTreeView(this);
    m_treeView->setObjectName(QStringLiteral("requirementsTree"));
    m_treeView->setModel(m_sortProxy);
    m_treeView->setRootIsDecorated(true);
    // Sorting is per section: the proxy keeps the section rows in the model's
    // order and sorts only the requirements inside them (REQ-SW-PL-001).
    m_treeView->setSortingEnabled(true);
    m_treeView->sortByColumn(RequirementsModel::IdColumn, Qt::AscendingOrder);
    m_treeView->setColumnWidth(RequirementsModel::IdColumn, 150);
    m_treeView->expandAll();

    m_preview = new QTextBrowser(this);
    m_preview->setObjectName(QStringLiteral("requirementPreview"));
    m_preview->setReadOnly(true);
    m_preview->setOpenLinks(false); // handle navigation ourselves

    m_editor = new QPlainTextEdit(this);
    m_editor->setVisible(false);

    m_fileLabel = new QLabel(this);
    m_validationLabel = new QLabel(this);
    m_validationLabel->setTextFormat(Qt::RichText);
    m_validationLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse
                                               | Qt::LinksAccessibleByKeyboard);
    m_validationLabel->setVisible(false);
    m_rootStatusLabel = new QLabel(this);

    m_editAction = new QAction(tr("Edit"), this);
    m_editAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+E")));
    m_saveAction = new QAction(tr("Save"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    m_cancelAction = new QAction(tr("Cancel"), this);
    m_browseAction = new QAction(tr("Add requirements folder…"), this);
    m_newAction = new QAction(tr("New Requirement…"), this);
    m_newAction->setShortcut(QKeySequence::New);
    m_doneAction = new QAction(tr("Mark Done & Archive"), this);
    m_reopenAction = new QAction(tr("Reopen"), this);
    m_acceptAction = new QAction(tr("Приеми"), this);
    m_acceptAction->setObjectName(QStringLiteral("acceptDraftAction"));
    m_acceptAction->setToolTip(
        tr("Одобрява избраната чернова (DRAFT → ACTIVE) — само човек го прави"));
    // The other two verdicts of a draft (REQ-SW-PL-102). They exist only for a
    // DRAFT, exactly like „Приеми“: a verdict needs something to judge.
    m_requestChangesAction = new QAction(tr("За корекция"), this);
    m_requestChangesAction->setObjectName(QStringLiteral("requestChangesAction"));
    m_requestChangesAction->setToolTip(
        tr("Записва кръг „за корекция“ с бележка — статусът остава DRAFT"));
    m_rejectAction = new QAction(tr("Откажи"), this);
    m_rejectAction->setObjectName(QStringLiteral("rejectDraftAction"));
    m_rejectAction->setToolTip(
        tr("Отказва черновата: CANCELLED, ред с причината и преместване в archive/"));
    // A contract change (REQ-SW-PL-101): an accepted requirement goes back to
    // DRAFT and needs a fresh approval.
    m_contractAction = new QAction(tr("Промени договора"), this);
    m_contractAction->setObjectName(QStringLiteral("changeContractAction"));
    m_contractAction->setToolTip(
        tr("Връща изискването на DRAFT с ред в Историята — иска ново одобрение"));
    m_depsAction = new QAction(tr("Edit deps…"), this);
    m_validateAction = new QAction(tr("Validate"), this);
    m_validateAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+V")));
    m_helpAction = new QAction(tr("Help / Format"), this);
    m_helpAction->setShortcut(QKeySequence::HelpContents);
    m_backAction = new QAction(tr("Back"), this);
    m_backAction->setShortcut(QKeySequence::Back);
    m_forwardAction = new QAction(tr("Forward"), this);
    m_forwardAction->setShortcut(QKeySequence::Forward);
    m_backAction->setEnabled(false);
    m_forwardAction->setEnabled(false);

    m_saveAction->setEnabled(false);
    m_cancelAction->setEnabled(false);
    m_doneAction->setEnabled(false);
    m_reopenAction->setEnabled(false);
    m_acceptAction->setEnabled(false);
    m_requestChangesAction->setEnabled(false);
    m_rejectAction->setEnabled(false);
    m_contractAction->setEnabled(false);
    m_depsAction->setEnabled(false);
    m_editAction->setEnabled(false);

    m_criteriaList = new QListWidget(this);
    m_criteriaList->setVisible(false);

    // Owner-facing counter (REQ-SW-PL-099): what is waiting for approval,
    // recomputed on every reload so it never needs the files to be opened.
    m_draftCountLabel = new QLabel(this);
    m_draftCountLabel->setObjectName(QStringLiteral("draftCountLabel"));
    m_draftCountLabel->setText(tr("чакат теб: 0 · за корекция: 0"));

    QHBoxLayout *treeControlsLayout = new QHBoxLayout;
    treeControlsLayout->addWidget(new QLabel(tr("View:"), this));
    treeControlsLayout->addWidget(m_viewModeCombo);
    treeControlsLayout->addWidget(new QLabel(tr("Repo:"), this));
    treeControlsLayout->addWidget(m_repoFilterCombo);
    treeControlsLayout->addWidget(new QLabel(tr("Search:"), this));
    treeControlsLayout->addWidget(m_searchEdit, 1);
    treeControlsLayout->addWidget(m_matchLabel);
    treeControlsLayout->addStretch();

    QVBoxLayout *treeLayout = new QVBoxLayout;
    treeLayout->addLayout(treeControlsLayout);
    treeLayout->addWidget(m_treeView);
    // The counter sits under the tree, not in the filter row: it is a reading,
    // not a control, and the filter row is what decides the panel's minimum
    // width (REQ-SW-PL-001 keeps the window narrow).
    treeLayout->addWidget(m_draftCountLabel, 0, Qt::AlignRight);

    QWidget *treePanel = new QWidget(this);
    treePanel->setLayout(treeLayout);

    QVBoxLayout *fileLayout = new QVBoxLayout;
    fileLayout->addWidget(m_fileLabel);
    fileLayout->addWidget(m_rootStatusLabel);

    // Actions in a text-only toolbar instead of a row of QPushButtons: the
    // toolbar overflows into an extension menu, so the minimum width is one
    // button plus the ellipsis rather than the sum of all eleven buttons
    // (REQ-SW-PL-001).
    m_actionBar = new QToolBar(tr("Actions"), this);
    m_actionBar->setObjectName(QStringLiteral("requirementsActionBar"));
    m_actionBar->setMovable(false);
    m_actionBar->setFloatable(false);
    m_actionBar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_actionBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_actionBar->addAction(m_backAction);
    m_actionBar->addAction(m_forwardAction);
    m_actionBar->addSeparator();
    m_actionBar->addAction(m_newAction);
    m_actionBar->addAction(m_editAction);
    m_actionBar->addAction(m_saveAction);
    m_actionBar->addAction(m_cancelAction);
    m_actionBar->addSeparator();
    m_actionBar->addAction(m_depsAction);
    m_actionBar->addAction(m_acceptAction);
    m_actionBar->addAction(m_requestChangesAction);
    m_actionBar->addAction(m_rejectAction);
    m_actionBar->addAction(m_contractAction);
    m_actionBar->addAction(m_doneAction);
    m_actionBar->addAction(m_reopenAction);
    m_actionBar->addSeparator();
    m_actionBar->addAction(m_validateAction);
    m_actionBar->addAction(m_helpAction);
    m_actionBar->addSeparator();
    m_actionBar->addAction(m_browseAction);

    fileLayout->addWidget(m_actionBar);

    fileLayout->addWidget(m_validationLabel);

    // Preview and editor share ONE pane: Edit only swaps which of the two is
    // visible, so the vertical splitter below keeps its sizes and handle in
    // place (REQ-SW-PL-095).
    QWidget *previewPane = new QWidget(this);
    previewPane->setObjectName(QStringLiteral("requirementPreviewPane"));
    QVBoxLayout *previewPaneLayout = new QVBoxLayout(previewPane);
    previewPaneLayout->setContentsMargins(0, 0, 0, 0);
    previewPaneLayout->addWidget(m_preview);
    previewPaneLayout->addWidget(m_editor);

    // The splitter only moves the boundary if both halves can actually shrink
    // and grow: a minimum keeps them usable, no maximum lets them take the
    // room the user gives them (REQ-SW-PL-095).
    m_preview->setMinimumHeight(80);
    m_editor->setMinimumHeight(80);
    m_criteriaList->setMinimumHeight(80);

    m_detailsSplitter = new QSplitter(Qt::Vertical, this);
    m_detailsSplitter->setObjectName(QStringLiteral("requirementDetailsSplitter"));
    m_detailsSplitter->setChildrenCollapsible(false);
    m_detailsSplitter->setHandleWidth(6);
    m_detailsSplitter->addWidget(previewPane);
    m_detailsSplitter->addWidget(m_criteriaList);
    // Preview gets ~2/3, the checkbox block ~1/3, and the ratio survives a
    // window resize until the user drags the handle.
    m_detailsSplitter->setStretchFactor(0, 2);
    m_detailsSplitter->setStretchFactor(1, 1);
    m_detailsSplitter->setSizes({400, 120});

    fileLayout->addWidget(m_detailsSplitter, 1);

    QWidget *detailsPanel = new QWidget(this);
    detailsPanel->setLayout(fileLayout);

    m_splitter = new QSplitter(this);
    m_splitter->addWidget(treePanel);
    m_splitter->addWidget(detailsPanel);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 3);
    m_splitter->setChildrenCollapsible(false);

    m_graphWidget = new DependencyGraphWidget(this);
    m_matrixWidget = new TraceabilityMatrixWidget(this);

    // "Идеи" tab (REQ-SW-PL-095): its own model + proxy, its own tab. Nothing
    // from here reaches m_requirements, the validator or the matrix.
    m_ideaModel = new IdeaTableModel(this);
    m_ideaProxy = new IdeaFilterProxyModel(this);
    m_ideaProxy->setSourceModel(m_ideaModel);
    m_ideasPanel = createIdeasPanel();

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(m_splitter, tr("Requirements"));
    m_tabs->addTab(m_graphWidget, tr("Dependency Graph"));
    m_tabs->addTab(m_matrixWidget, tr("Traceability"));
    m_ideasTabIndex = m_tabs->addTab(m_ideasPanel, tr("Идеи"));
    m_tabs->setTabVisible(m_ideasTabIndex, false); // shown only when ideas exist

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_tabs);
    setLayout(mainLayout);

    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &RequirementsWidget::onSelectionChanged);
    connect(m_editAction, &QAction::triggered, this,
            [this]() { onEditToggled(!m_editor->isVisible()); });
    connect(m_saveAction, &QAction::triggered, this, &RequirementsWidget::onSave);
    connect(m_cancelAction, &QAction::triggered, this, &RequirementsWidget::onCancel);
    connect(m_browseAction, &QAction::triggered, this, &RequirementsWidget::onBrowseDirectory);
    connect(m_newAction, &QAction::triggered, this, &RequirementsWidget::onNewRequirement);
    connect(m_backAction, &QAction::triggered, this, &RequirementsWidget::onNavBack);
    connect(m_forwardAction, &QAction::triggered, this, &RequirementsWidget::onNavForward);
    connect(m_doneAction, &QAction::triggered, this, &RequirementsWidget::onMarkDoneAndArchive);
    connect(m_reopenAction, &QAction::triggered, this, &RequirementsWidget::onReopen);
    connect(m_acceptAction, &QAction::triggered, this, &RequirementsWidget::onAcceptDraft);
    connect(m_requestChangesAction, &QAction::triggered,
            this, &RequirementsWidget::onRequestChanges);
    connect(m_rejectAction, &QAction::triggered, this, &RequirementsWidget::onRejectDraft);
    connect(m_contractAction, &QAction::triggered, this, &RequirementsWidget::onChangeContract);
    connect(m_depsAction, &QAction::triggered, this, &RequirementsWidget::onEditDependencies);
    connect(m_validateAction, &QAction::triggered, this, &RequirementsWidget::onValidate);
    connect(m_helpAction, &QAction::triggered, this, &RequirementsWidget::onShowHelp);
    connect(m_preview, &QTextBrowser::anchorClicked,
            this, &RequirementsWidget::onAnchorClicked);
    connect(m_validationLabel, &QLabel::linkActivated,
            this, [this](const QString &) { onValidate(); });
    connect(m_viewModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RequirementsWidget::onViewModeChanged);
    connect(m_repoFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RequirementsWidget::onRepoFilterChanged);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &RequirementsWidget::onSearchTextChanged);
    connect(m_searchTimer, &QTimer::timeout, this, [this]() { applyViewFilters(); });
    connect(m_graphWidget, &DependencyGraphWidget::navigateRequested,
            this, &RequirementsWidget::onGraphNavigateRequested);
    connect(m_criteriaList, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        const int index = item->data(Qt::UserRole).toInt();
        onCriterionToggled(index, item->checkState() == Qt::Checked);
    });

    // External changes (git pull, a rename, a file created by hand) must reach
    // the list without a restart (REQ-SW-PL-102 AC7). Only DIRECTORIES are
    // watched: a content write — ours included — does not change the set, and
    // watching files would reload the tree twice for every status rewrite.
    m_watcher = new QFileSystemWatcher(this);
    m_watchTimer = new QTimer(this);
    m_watchTimer->setSingleShot(true);
    m_watchTimer->setInterval(300);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &RequirementsWidget::onWatchedPathChanged);
    connect(m_watchTimer, &QTimer::timeout, this, [this]() { reload(); });
}

RequirementsWidget::~RequirementsWidget() = default;

void RequirementsWidget::openDirectory(const QString &baseDir)
{
    openDirectories({baseDir});
}

void RequirementsWidget::openDirectories(const QStringList &baseDirs)
{
    // APPEND roots (never replace): the browse button keeps adding folders.
    for (const QString &dir : baseDirs) {
        const QString canonical = QDir(dir).canonicalPath();
        if (canonical.isEmpty() || m_roots.contains(canonical))
            continue;
        // Warn when the new root is inside (or contains) an already-loaded
        // root: those files overlap and the parser dedups them by canonical
        // file path, but the overlap is usually a user mistake.
        const QString separator = QDir::separator();
        for (const QString &existing : m_roots) {
            if (canonical.startsWith(existing + separator)
                || existing.startsWith(canonical + separator)) {
                qCWarning(lcFramework)
                    << "RequirementsManager: new root overlaps an already-loaded root"
                    << canonical << "vs" << existing
                    << "- files reachable via both roots are parsed once (dedup by file path)";
                break;
            }
        }
        m_roots.append(canonical);
    }
    std::sort(m_roots.begin(), m_roots.end());

    if (!m_roots.isEmpty())
        m_baseDir = m_roots.first();
    reload();
    qCInfo(lcFramework) << "RequirementsManager: opened requirements roots"
                        << m_roots;
}

QString RequirementsWidget::baseDirectory() const
{
    return m_baseDir;
}

QVector<Requirement> RequirementsWidget::filterRequirementsByRepo(
    const QVector<Requirement> &requirements, const QString &repo)
{
    const QString trimmed = repo.trimmed();
    if (trimmed.isEmpty() || trimmed == QStringLiteral("All"))
        return requirements;

    QVector<Requirement> filtered;
    for (const Requirement &req : requirements) {
        if (QString::compare(req.repo, trimmed, Qt::CaseInsensitive) == 0)
            filtered.append(req);
    }
    return filtered;
}

void RequirementsWidget::refreshRepoFilterCombo()
{
    // "All repos" + "public" + "private" + one entry per discovered root when
    // its label differs from the generic repo labels.
    const QString previous = m_repoFilterCombo->currentData().toString();
    QStringList usedLabels;
    usedLabels << QStringLiteral("public") << QStringLiteral("private");

    m_repoFilterCombo->blockSignals(true);
    m_repoFilterCombo->clear();
    m_repoFilterCombo->addItem(tr("All repos"), QString());
    m_repoFilterCombo->addItem(QStringLiteral("public"), QStringLiteral("public"));
    m_repoFilterCombo->addItem(QStringLiteral("private"), QStringLiteral("private"));

    for (const QString &root : m_roots) {
        const QString label = QDir(root).dirName();
        if (label.isEmpty() || usedLabels.contains(label, Qt::CaseInsensitive))
            continue;
        usedLabels.append(label);
        // Data = the root path; onRepoFilterChanged treats root entries as
        // filePath-prefix filters.
        m_repoFilterCombo->addItem(label, root);
    }

    const int restore = m_repoFilterCombo->findData(previous);
    m_repoFilterCombo->setCurrentIndex(restore >= 0 ? restore : 0);
    m_repoFilterCombo->blockSignals(false);
}

void RequirementsWidget::reload()
{
    // A reload caused by the watcher must not arm the debounce again: the
    // timer's own tick is not a change that needs another reload.
    if (m_watchTimer)
        m_watchTimer->stop();

    QVector<RequirementRoot> roots;
    for (const QString &root : m_roots)
        roots.append(RequirementRoot{root});
    m_requirements = RequirementsParser::parseDirectories(roots);

    // The reference day moves with the wall clock, the model does not: it is
    // built once, so without this line the age column and the overdue colour
    // stay on the day the manager was started while the counter's tooltip
    // computes with today (REQ-SW-PL-100). Reload is the single place that
    // knows "the world changed".
    if (m_model)
        m_model->setToday(QDate::currentDate());

    refreshRepoFilterCombo();
    onRepoFilterChanged(m_repoFilterCombo->currentIndex());

    m_currentIndex = -1;
    m_preview->clear();
    m_editor->clear();
    m_fileLabel->clear();
    m_criteriaList->clear();
    m_treeView->expandAll();

    m_validationIssues = RequirementsValidator::validate(m_requirements);
    m_rootStatusLabel->setText(
        tr("Roots loaded: %1  |  Requirements: %2")
            .arg(m_roots.size())
            .arg(m_requirements.size()));

    // Recomputed on every reload (REQ-SW-PL-099): a draft is visible without
    // opening a single file. Since REQ-SW-PL-100 it also says how long the
    // oldest one has waited, so an overdue proposal cannot hide behind a count.
    //
    // REQ-SW-PL-102 AC4 splits the number in two: a draft nobody has judged
    // yet ("чакат теб") is a different thing from one that was read and sent
    // back ("за корекция"). The review verdict comes from the model, computed
    // once per load — the counter never parses Markdown.
    int waiting = 0;
    int changesRequested = 0;
    int drafts = 0;
    int oldestDays = -1;
    QString oldestId;
    const QDate today = QDate::currentDate();
    for (int i = 0; i < m_requirements.size(); ++i) {
        const Requirement &req = m_requirements.at(i);
        if (RequirementsValidator::normalizedStatus(req.status)
            != QStringLiteral("DRAFT")) {
            continue;
        }
        ++drafts;
        const QString verdict = RequirementsParser::lastDraftVerdict(req.rawContent);
        if (verdict == RequirementsParser::changesRequestedVerdict())
            ++changesRequested;
        else if (verdict.isEmpty())
            ++waiting;
        const int days = RequirementDates::ageInDays(createdDate(req), today);
        // A missing date is "unknown" (-1) and must not win the comparison; it
        // is still reported when it is the only draft (oldestId stays empty
        // until the first draft is seen).
        if (oldestId.isEmpty() || days > oldestDays) {
            oldestDays = days;
            oldestId = req.id;
        }
    }
    m_draftCountLabel->setText(tr("чакат теб: %1 · за корекция: %2")
                                   .arg(waiting)
                                   .arg(changesRequested));
    if (drafts == 0) {
        m_draftCountLabel->setToolTip(tr("Няма чернови, чакащи одобрение."));
    } else if (oldestDays >= 0) {
        m_draftCountLabel->setToolTip(
            tr("Чернови: %1. Най-старата чака от %2 дни: %3")
                .arg(drafts)
                .arg(oldestDays)
                .arg(oldestId));
    } else {
        m_draftCountLabel->setToolTip(
            tr("Чернови: %1. Най-старата е %2 — датата липсва или е невалидна")
                .arg(drafts)
                .arg(oldestId));
    }
    m_graphWidget->setRequirements(m_filtered, m_validationIssues);
    updateValidationStatus();
    refreshActionState();
    refreshIdeasTab();
    // The set of directories may have changed (a new subdirectory, a root
    // added): the watcher is rebuilt from what is on disk right now.
    rebuildWatchedPaths();
}

void RequirementsWidget::rebuildWatchedPaths()
{
    if (!m_watcher)
        return;

    QStringList wanted;
    for (const QString &root : m_roots) {
        const QString tree =
            QDir(root).filePath(QString::fromUtf8(kRequirementsSubdir));
        const QString walkRoot = QDir(tree).exists() ? tree : root;
        wanted.append(QDir::toNativeSeparators(QDir(walkRoot).absolutePath()));
        // QFileSystemWatcher is not recursive: every level is its own entry.
        QDirIterator it(walkRoot, QDir::Dirs | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext())
            wanted.append(QDir::toNativeSeparators(it.next()));
    }
    wanted.removeDuplicates();

    for (const QString &dir : m_watcher->directories()) {
        if (!wanted.contains(dir))
            m_watcher->removePath(dir);
    }
    for (const QString &dir : wanted) {
        if (!m_watcher->directories().contains(dir) && QDir(dir).exists())
            m_watcher->addPath(dir);
    }
}

void RequirementsWidget::onWatchedPathChanged()
{
    // One reload per burst: git pull rewrites a folder, not a file.
    if (m_watchTimer && !m_watchTimer->isActive())
        m_watchTimer->start();
}

const Requirement *RequirementsWidget::requirementAtViewIndex(const QModelIndex &viewIndex) const
{
    if (!viewIndex.isValid())
        return nullptr;
    // The tree shows the sorted proxy; the model owns the Requirement records.
    return m_model->requirementAt(m_sortProxy->mapToSource(viewIndex));
}

void RequirementsWidget::onSelectionChanged()
{
    const QModelIndex index = m_treeView->currentIndex();
    const Requirement *req = requirementAtViewIndex(index);
    if (!req) {
        m_currentIndex = -1;
        m_preview->clear();
        m_editor->clear();
        m_fileLabel->clear();
        m_criteriaList->clear();
        refreshActionState();
        return;
    }

    for (int i = 0; i < m_requirements.size(); ++i) {
        if (m_requirements.at(i).filePath == req->filePath) {
            m_currentIndex = i;
            break;
        }
    }

    // The row came from the model, which holds the FILTERED subset: if the
    // full set does not carry that path, there is no index to use and
    // m_requirements.at(-1) would be out of bounds.
    if (m_currentIndex < 0) {
        m_preview->clear();
        m_editor->clear();
        m_fileLabel->clear();
        m_criteriaList->clear();
        refreshActionState();
        return;
    }

    if (m_editor->isVisible()) {
        m_editor->setPlainText(m_requirements.at(m_currentIndex).rawContent);
    }
    showPreview();
    refreshActionState();
}

void RequirementsWidget::onEditToggled(bool edit)
{
    if (m_currentIndex < 0)
        return;

    if (edit) {
        m_editor->setVisible(true);
        m_preview->setVisible(false);
        m_criteriaList->setVisible(false);
        m_editor->setPlainText(m_requirements.at(m_currentIndex).rawContent);
        m_saveAction->setEnabled(true);
        m_cancelAction->setEnabled(true);
        m_editAction->setText(tr("Preview"));
    } else {
        showPreview();
        m_editAction->setText(tr("Edit"));
        m_saveAction->setEnabled(false);
        m_cancelAction->setEnabled(false);
    }
}

void RequirementsWidget::onSave()
{
    if (m_currentIndex < 0)
        return;

    Requirement &req = m_requirements[m_currentIndex];
    req.rawContent = m_editor->toPlainText();
    if (!RequirementsParser::writeRequirement(req)) {
        QMessageBox::warning(this, tr("Requirements"),
                             tr("Failed to write requirement file:\n%1").arg(req.filePath));
        return;
    }
    reload();
    m_editAction->setText(tr("Edit"));
    m_saveAction->setEnabled(false);
    m_cancelAction->setEnabled(false);
}

void RequirementsWidget::onCancel()
{
    if (m_currentIndex < 0)
        return;
    showPreview();
    m_editAction->setText(tr("Edit"));
    m_saveAction->setEnabled(false);
    m_cancelAction->setEnabled(false);
}

void RequirementsWidget::onCriterionToggled(int index, bool done)
{
    if (m_currentIndex < 0)
        return;

    Requirement &req = m_requirements[m_currentIndex];
    RequirementsParser::setCriterionChecked(req, index, done);
    RequirementsParser::writeRequirement(req);
}

void RequirementsWidget::onBrowseDirectory()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Add requirements folder"), m_baseDir);
    if (dir.isEmpty())
        return;
    openDirectories({dir});
}

void RequirementsWidget::onNewRequirement()
{
    NewRequirementDialog dialog(m_baseDir, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString createdPath = dialog.createdFilePath();
    reload();
    for (const Requirement &req : m_requirements) {
        if (req.filePath == createdPath) {
            navigateToId(req.id);
            break;
        }
    }
}

void RequirementsWidget::onMarkDoneAndArchive()
{
    if (m_currentIndex < 0)
        return;

    Requirement &req = m_requirements[m_currentIndex];
    if (req.section != QStringLiteral("active"))
        return;

    QStringList unfinished;
    for (const QString &dep : req.dependencies) {
        const Requirement *target = nullptr;
        for (const Requirement &candidate : m_requirements) {
            if (candidate.id == dep) {
                target = &candidate;
                break;
            }
        }
        if (!target) {
            unfinished.append(QStringLiteral("%1 (missing)").arg(dep));
        } else if (target->status != QStringLiteral("DONE")) {
            unfinished.append(QStringLiteral("%1 (%2)").arg(dep, target->status));
        }
    }

    if (!unfinished.isEmpty()) {
        QMessageBox::warning(
            this, tr("Cannot Archive"),
            tr("Requirement %1 has unfinished dependencies:\n%2\n\n"
               "Finish or resolve them before archiving.")
                .arg(req.id, unfinished.join(QStringLiteral("\n"))));
        return;
    }

    RequirementsParser::setStatusLine(req, QStringLiteral("DONE"));
    // The status changed, so the file must say why in the same edit: the whole
    // point of the history guard (REQ-SW-PL-101).
    RequirementsParser::appendHistoryRow(req, QDate::currentDate().toString(Qt::ISODate),
                                         QStringLiteral("DONE — приключено и архивирано"),
                                         QStringLiteral("потребител"),
                                         QStringLiteral("всички зависимости са готови"));
    if (!RequirementsParser::writeRequirement(req)) {
        QMessageBox::warning(this, tr("Requirements"),
                             tr("Failed to write requirement file:\n%1").arg(req.filePath));
        return;
    }
    if (!RequirementsParser::moveToArchive(req.filePath)) {
        QMessageBox::warning(this, tr("Requirements"),
                             tr("Failed to move file to archive:\n%1").arg(req.filePath));
        return;
    }
    reload();
}

void RequirementsWidget::onReopen()
{
    if (m_currentIndex < 0)
        return;

    Requirement &req = m_requirements[m_currentIndex];
    if (req.section != QStringLiteral("archive"))
        return;

    RequirementsParser::setStatusLine(req, QStringLiteral("ACTIVE"));
    RequirementsParser::appendHistoryRow(req, QDate::currentDate().toString(Qt::ISODate),
                                         QStringLiteral("ACTIVE — върнато от архива"),
                                         QStringLiteral("потребител"),
                                         QStringLiteral("работата по него продължава"));
    if (!RequirementsParser::writeRequirement(req)) {
        QMessageBox::warning(this, tr("Requirements"),
                             tr("Failed to write requirement file:\n%1").arg(req.filePath));
        return;
    }
    if (!RequirementsParser::moveToActive(req.filePath)) {
        QMessageBox::warning(this, tr("Requirements"),
                             tr("Failed to move file to active:\n%1").arg(req.filePath));
        return;
    }
    reload();
}

void RequirementsWidget::onAcceptDraft()
{
    if (m_currentIndex < 0)
        return;
    const Requirement &req = m_requirements.at(m_currentIndex);

    // The confirmation is the whole point of the action: an approval is what
    // allows implementation, so it is asked for explicitly and never silent.
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Приеми изискването?"),
        tr("Това одобрение позволява имплементация. Сигурен ли си?\n\n%1")
            .arg(req.id),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    // By value on purpose: `req` aliases m_requirements, which acceptDraft()
    // reloads. The copy is taken here, before anything is invalidated.
    const QString id = req.id;
    acceptDraft(id, QDate::currentDate().toString(Qt::ISODate));
}

bool RequirementsWidget::acceptDraft(QString id, const QString &date)
{
    // `id` is a copy (see the header): reload() below reallocates
    // m_requirements, so neither a reference to an element nor a pointer into
    // it may survive the call.
    const QString filePath = resolveRequirementFile(id);
    if (filePath.isEmpty()) {
        qCWarning(lcFramework) << "RequirementsManager: няма изискване" << id;
        missingFile(id);
        return false;
    }

    QString error;
    if (!RequirementsParser::approveDraft(filePath, date, &error)) {
        QMessageBox::warning(this, tr("Requirements"),
                             tr("Файлът не можа да се презапише:\n%1").arg(error));
        qCWarning(lcFramework) << "RequirementsManager: одобрението се провали за"
                               << id << error;
        return false;
    }

    qCInfo(lcFramework) << "RequirementsManager: одобрено изискване" << id
                        << "(DRAFT → ACTIVE," << date + QLatin1Char(')');
    reload();
    // `id` is a local copy, so it survives the reload above.
    navigateToId(id);
    return true;
}

QString RequirementsWidget::missingFileMessage(const QString &id)
{
    // REQ-SW-PL-102 AC7: "no such file" is a dead end; the sentence names the
    // requirement, both possible causes and the one action that helps.
    return tr("Изискването %1 не е намерено по ID-то в заглавието на файла — "
              "файлът е преименуван или преместен — презареди.")
        .arg(id);
}

QString RequirementsWidget::resolveRequirementFile(const QString &id) const
{
    QVector<RequirementRoot> roots;
    for (const QString &root : m_roots)
        roots.append(RequirementRoot{root});
    // A fresh walk of the tree on disk, never `req.filePath` from the cached
    // vector: the H1 is the identity, the path is not (REQ-SW-PL-102 AC7).
    return RequirementsParser::findFileById(roots, id);
}

void RequirementsWidget::onRequestChanges()
{
    if (m_currentIndex < 0)
        return;
    const QString id = m_requirements.at(m_currentIndex).id;

    DraftNoteDialog dialog(
        tr("За корекция — %1").arg(id),
        tr("Какво липсва, за да стане готово? Бележката остава в изискването "
           "като ред „за корекция“."),
        tr("Запиши присъдата"), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    requestChanges(id, dialog.note(), QDate::currentDate().toString(Qt::ISODate));
}

bool RequirementsWidget::requestChanges(QString id, const QString &note,
                                        const QString &date)
{
    const QString filePath = resolveRequirementFile(id);
    if (filePath.isEmpty()) {
        missingFile(id);
        return false;
    }

    QString error;
    if (!RequirementsParser::requestChangesDraft(filePath, note, date, &error)) {
        QMessageBox::warning(this, tr("Requirements"),
                             tr("Присъдата не можа да се запише:\n%1").arg(error));
        qCWarning(lcFramework) << "RequirementsManager: „за корекция“ се провали за"
                               << id << error;
        return false;
    }
    qCInfo(lcFramework) << "RequirementsManager: присъда „за корекция“ за" << id;
    reload();
    navigateToId(id);
    return true;
}

void RequirementsWidget::onRejectDraft()
{
    if (m_currentIndex < 0)
        return;
    const QString id = m_requirements.at(m_currentIndex).id;

    DraftNoteDialog dialog(
        tr("Откажи — %1").arg(id),
        tr("Защо се отказва? Причината остава в изискването и файлът се мести "
           "в archive/."),
        tr("Откажи черновата"), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    rejectDraft(id, dialog.note(), QDate::currentDate().toString(Qt::ISODate));
}

bool RequirementsWidget::rejectDraft(QString id, const QString &reason,
                                     const QString &date)
{
    const QString filePath = resolveRequirementFile(id);
    if (filePath.isEmpty()) {
        missingFile(id);
        return false;
    }
    // A refusal is final for this file: it is the one action the person may
    // want to think twice about.
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Откажи черновата?"),
        tr("Отказът мести файла в archive/ и статусът става CANCELLED.\n\n%1")
            .arg(id),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return false;

    QString error;
    if (!RequirementsParser::rejectDraft(filePath, reason, date, &error)) {
        QMessageBox::warning(this, tr("Requirements"),
                             tr("Отказът не можа да се запише:\n%1").arg(error));
        qCWarning(lcFramework) << "RequirementsManager: отказът се провали за"
                               << id << error;
        return false;
    }
    qCInfo(lcFramework) << "RequirementsManager: отказано изискване" << id;
    reload();
    navigateToId(id);
    return true;
}

void RequirementsWidget::onChangeContract()
{
    if (m_currentIndex < 0)
        return;
    const QString id = m_requirements.at(m_currentIndex).id;

    DraftNoteDialog dialog(
        tr("Промени договора — %1").arg(id),
        tr("Какво се добавя, маха или променя? Изискването се връща на DRAFT и "
           "иска ново одобрение; причината остава в Историята."),
        tr("Върни на DRAFT"), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    changeContract(id, dialog.note(), QDate::currentDate().toString(Qt::ISODate));
}

bool RequirementsWidget::changeContract(QString id, const QString &reason,
                                        const QString &date)
{
    const QString filePath = resolveRequirementFile(id);
    if (filePath.isEmpty()) {
        missingFile(id);
        return false;
    }

    QString error;
    if (!RequirementsParser::startContractChange(filePath, reason,
                                                 tr("потребител"), date, &error)) {
        QMessageBox::warning(this, tr("Requirements"),
                             tr("Промяната не можа да се запише:\n%1").arg(error));
        qCWarning(lcFramework) << "RequirementsManager: промяна в договора се провали за"
                               << id << error;
        return false;
    }
    qCInfo(lcFramework) << "RequirementsManager: договорът на" << id
                        << "е променен — обратно на DRAFT";
    reload();
    navigateToId(id);
    return true;
}

void RequirementsWidget::missingFile(const QString &id)
{
    const QString message = missingFileMessage(id);
    qCWarning(lcFramework) << "RequirementsManager:" << message;
    QMessageBox::warning(this, tr("Requirements"), message);
}

void RequirementsWidget::onEditDependencies()
{
    if (m_currentIndex < 0)
        return;

    const Requirement &req = m_requirements.at(m_currentIndex);

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Edit dependencies — %1").arg(req.id));
    dialog.resize(460, 380);

    QListWidget *list = new QListWidget(&dialog);
    for (const Requirement &other : m_requirements) {
        if (other.id == req.id)
            continue;
        QListWidgetItem *item = new QListWidgetItem(
            QStringLiteral("%1  %2").arg(other.id, other.title), list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(req.dependencies.contains(other.id)
                                ? Qt::Checked
                                : Qt::Unchecked);
        item->setData(Qt::UserRole, other.id);
    }

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Save)->setText(tr("Save"));
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addWidget(list);
    layout->addWidget(buttons);
    dialog.setLayout(layout);

    if (dialog.exec() != QDialog::Accepted)
        return;

    QStringList dependencies;
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem *item = list->item(i);
        if (item->checkState() == Qt::Checked)
            dependencies.append(item->data(Qt::UserRole).toString());
    }

    Requirement &mut = m_requirements[m_currentIndex];
    RequirementsParser::setDependenciesLine(mut, dependencies);
    if (!RequirementsParser::writeRequirement(mut)) {
        QMessageBox::warning(this, tr("Requirements"),
                             tr("Failed to write requirement file:\n%1").arg(mut.filePath));
        return;
    }
    reload();
}

void RequirementsWidget::onValidate()
{
    ValidationDialog dialog(m_validationIssues, this);
    connect(&dialog, &ValidationDialog::navigateRequested,
            this, [this](const QString &id) { navigateToId(id); });
    dialog.exec();
}

void RequirementsWidget::onShowHelp()
{
    HelpDialog dialog(this);
    dialog.exec();
}

void RequirementsWidget::onAnchorClicked(const QUrl &link)
{
    if (link.scheme() != QStringLiteral("req"))
        return;
    navigateToId(link.path());
}

void RequirementsWidget::onViewModeChanged(int index)
{
    const RequirementsModel::ViewMode mode =
        (index == 1) ? RequirementsModel::ViewMode::Hierarchy
                     : RequirementsModel::ViewMode::Sections;
    m_model->setViewMode(mode);
    m_treeView->expandAll();
}

void RequirementsWidget::onSearchTextChanged(const QString &text)
{
    m_searchQuery = text.trimmed();
    m_searchTimer->start();
}

void RequirementsWidget::applyViewFilters()
{
    const QString value = m_repoFilterCombo->itemData(
        m_repoFilterCombo->currentIndex()).toString();

    QVector<Requirement> repoFiltered;
    if (m_roots.contains(value)) {
        // A discovered-root entry filters by the root's file path prefix.
        for (const Requirement &req : m_requirements) {
            if (req.filePath.startsWith(value))
                repoFiltered.append(req);
        }
    } else {
        // Generic repo entry ("public"/"private") or "All repos" (empty).
        repoFiltered = filterRequirementsByRepo(m_requirements, value);
    }

    // The search narrows the repo-filtered subset (REQ-SW-PL-011). The
    // FILTERED subset feeds model/graph/matrix; the FULL set stays for
    // validation, preview and edit actions.
    m_filtered = RequirementsSearchEngine::filter(repoFiltered, m_searchQuery);
    m_model->setRequirements(m_filtered);
    m_matrixWidget->setRequirements(m_filtered);
    m_graphWidget->setRequirements(m_filtered, m_validationIssues);

    // Clear any stale selection: m_currentIndex indexes into the FULL set and
    // could point at a row filtered out of the current view.
    m_currentIndex = -1;
    m_preview->clear();
    m_editor->clear();
    m_fileLabel->clear();
    m_criteriaList->clear();
    m_treeView->expandAll();
    refreshActionState();

    m_matchLabel->setText(m_filtered.size() == 1
                              ? tr("%1 match").arg(m_filtered.size())
                              : tr("%1 matches").arg(m_filtered.size()));
    m_matchLabel->setVisible(!m_searchQuery.isEmpty());
}

void RequirementsWidget::onRepoFilterChanged(int index)
{
    Q_UNUSED(index);
    applyViewFilters();
}

void RequirementsWidget::onGraphNavigateRequested(const QString &id)
{
    // Jump back to the tree tab and select the requirement there.
    m_tabs->setCurrentIndex(0);
    navigateToId(id);
}

void RequirementsWidget::showPreview()
{
    m_editor->setVisible(false);
    m_preview->setVisible(true);
    m_criteriaList->setVisible(true);

    if (m_currentIndex < 0)
        return;

    const Requirement &req = m_requirements.at(m_currentIndex);
    m_fileLabel->setText(tr("File: %1  [%2]").arg(req.filePath, req.section));
    updatePreviewText(req);

    m_criteriaList->blockSignals(true);
    m_criteriaList->clear();
    for (int i = 0; i < req.acceptanceCriteria.size(); ++i) {
        QListWidgetItem *item = new QListWidgetItem(req.acceptanceCriteria.at(i), m_criteriaList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(req.criteriaDone.at(i) ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, i);
    }
    m_criteriaList->blockSignals(false);
}

void RequirementsWidget::updatePreviewText(const Requirement &req)
{
    // The whole file is rendered, not just the parsed fields: the header keeps
    // the at-a-glance metadata, everything below it is the real Markdown of the
    // .md (REQ-SW-PL-001). Relationship lists are gathered from the FULL set so
    // a filtered view does not hide a link.
    QStringList childIds;
    for (const Requirement &other : m_requirements) {
        if (other.parentId == req.id)
            childIds.append(other.id);
    }

    QStringList reverseIds;
    for (const Requirement &other : m_requirements) {
        if (other.dependencies.contains(req.id))
            reverseIds.append(other.id);
    }

    RequirementPreview::render(m_preview, req, childIds, reverseIds);
}

void RequirementsWidget::navigateToId(const QString &id, bool addToHistory)
{
    // An empty (or blank) id is not a requirement: stay quiet instead of
    // raising a "not found" message that names nothing.
    if (id.trimmed().isEmpty())
        return;
    // indexForId() speaks source-model indexes; the view shows the sorted
    // proxy, so every index crosses that boundary once.
    const QModelIndex index = m_sortProxy->mapFromSource(m_model->indexForId(id));
    if (!index.isValid()) {
        if (!m_searchQuery.isEmpty()) {
            m_searchQuery.clear();
            m_searchEdit->clear();
            applyViewFilters();
            const QModelIndex retryIndex =
                m_sortProxy->mapFromSource(m_model->indexForId(id));
            if (retryIndex.isValid()) {
                m_treeView->expandAll();
                m_treeView->scrollTo(retryIndex);
                m_treeView->setCurrentIndex(retryIndex);
                m_treeView->selectionModel()->select(
                    retryIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
                if (addToHistory)
                    pushNavHistory(id);
                return;
            }
        }
        QMessageBox::information(this, tr("Requirements"),
                                 tr("Requirement '%1' not found.").arg(id));
        return;
    }
    m_treeView->expandAll();
    m_treeView->scrollTo(index);
    m_treeView->setCurrentIndex(index);
    m_treeView->selectionModel()->select(
        index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    if (addToHistory)
        pushNavHistory(id);
}

void RequirementsWidget::updateValidationStatus()
{
    const int errors = RequirementsValidator::countSeverity(
        m_validationIssues, RequirementsValidator::Severity::Error);
    const int warnings = RequirementsValidator::countSeverity(
        m_validationIssues, RequirementsValidator::Severity::Warning);

    QString text;
    if (errors == 0 && warnings == 0) {
        text = QStringLiteral("<span style=\"color:green;\">&#10003; Validation: OK</span>");
    } else if (errors == 0) {
        text = QStringLiteral("<span style=\"color:#C07A00;\">Validation: %1 warning(s)"
                              " &mdash; <a href=\"validate\">view report</a></span>")
                   .arg(warnings);
    } else {
        text = QStringLiteral("<span style=\"color:red;\">Validation: %1 error(s), %2 warning(s)"
                              " &mdash; <a href=\"validate\">view report</a></span>")
                   .arg(errors)
                   .arg(warnings);
    }
    m_validationLabel->setText(text);
    m_validationLabel->setVisible(true);
}

void RequirementsWidget::refreshActionState()
{
    const bool valid = m_currentIndex >= 0
                       && m_currentIndex < m_requirements.size();
    const bool active = valid && m_requirements.at(m_currentIndex).section
                                        == QStringLiteral("active");
    const bool archived = valid && m_requirements.at(m_currentIndex).section
                                        == QStringLiteral("archive");
    const bool draft = valid
        && RequirementsValidator::normalizedStatus(
               m_requirements.at(m_currentIndex).status) == QStringLiteral("DRAFT");

    m_editAction->setEnabled(valid);
    m_depsAction->setEnabled(valid);
    m_doneAction->setEnabled(active);
    m_reopenAction->setEnabled(archived);
    // Accepting is the owner's act and exists for exactly one transition:
    // DRAFT -> ACTIVE inside active/ (REQ-SW-PL-099). The other two verdicts
    // (REQ-SW-PL-102) judge the same object, so they share the condition.
    m_acceptAction->setEnabled(draft && active);
    m_requestChangesAction->setEnabled(draft && active);
    m_rejectAction->setEnabled(draft && active);
    // A contract change needs a contract: an accepted requirement, ACTIVE or
    // IMPLEMENTED (REQ-SW-PL-101 AC4) — never a draft, never a cancelled one.
    const QString token = valid
        ? RequirementsValidator::normalizedStatus(m_requirements.at(m_currentIndex).status)
        : QString();
    m_contractAction->setEnabled(active
                                 && (token == QStringLiteral("ACTIVE")
                                     || token == QStringLiteral("IMPLEMENTED")));
    updateNavButtons();
}

void RequirementsWidget::pushNavHistory(const QString &id)
{
    // Truncate any forward history if we're not at the end.
    if (m_navHistoryPos < m_navHistory.size() - 1)
        m_navHistory.resize(m_navHistoryPos + 1);

    // Don't add duplicate if same as current position.
    if (m_navHistoryPos >= 0 && m_navHistory.at(m_navHistoryPos) == id)
        return;

    m_navHistory.append(id);
    m_navHistoryPos = m_navHistory.size() - 1;
    updateNavButtons();
}

void RequirementsWidget::onNavBack()
{
    if (m_navHistoryPos > 0) {
        --m_navHistoryPos;
        navigateToId(m_navHistory.at(m_navHistoryPos), false);
        updateNavButtons();
    }
}

void RequirementsWidget::onNavForward()
{
    if (m_navHistoryPos < m_navHistory.size() - 1) {
        ++m_navHistoryPos;
        navigateToId(m_navHistory.at(m_navHistoryPos), false);
        updateNavButtons();
    }
}

void RequirementsWidget::updateNavButtons()
{
    m_backAction->setEnabled(m_navHistoryPos > 0);
    m_forwardAction->setEnabled(m_navHistoryPos < m_navHistory.size() - 1);
}

QWidget *RequirementsWidget::createIdeasPanel()
{
    m_ideasSearchEdit = new QLineEdit(this);
    m_ideasSearchEdit->setObjectName(QStringLiteral("ideasSearch"));
    m_ideasSearchEdit->setClearButtonEnabled(true);
    m_ideasSearchEdit->setPlaceholderText(tr("Search ideas by ID or title…"));

    m_promoteButton = new QPushButton(tr("Създай изискване от тази идея"), this);
    m_promoteButton->setObjectName(QStringLiteral("promoteIdeaButton"));
    m_promoteButton->setEnabled(false);

    QHBoxLayout *controlsLayout = new QHBoxLayout;
    controlsLayout->addWidget(new QLabel(tr("Search:"), this));
    controlsLayout->addWidget(m_ideasSearchEdit, 1);
    controlsLayout->addWidget(m_promoteButton);

    m_ideasView = new QTableView(this);
    m_ideasView->setObjectName(QStringLiteral("ideasTable"));
    m_ideasView->setModel(m_ideaProxy);
    // Same proxy approach as the requirements tree: sorting lives in the view,
    // the model stays in register order (REQ-SW-PL-095).
    m_ideasView->setSortingEnabled(true);
    m_ideasView->sortByColumn(IdeaTableModel::IdColumn, Qt::AscendingOrder);
    m_ideasView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ideasView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ideasView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_ideasView->setAlternatingRowColors(true);
    m_ideasView->verticalHeader()->setVisible(false);
    m_ideasView->horizontalHeader()->setSectionResizeMode(IdeaTableModel::TitleColumn,
                                                         QHeaderView::Stretch);

    m_ideaPreview = new QTextBrowser(this);
    m_ideaPreview->setObjectName(QStringLiteral("ideaPreview"));
    m_ideaPreview->setReadOnly(true);
    m_ideaPreview->setOpenLinks(false);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_ideasView);
    splitter->addWidget(m_ideaPreview);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setChildrenCollapsible(false);

    QWidget *panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("ideasPanel"));
    QVBoxLayout *panelLayout = new QVBoxLayout(panel);
    panelLayout->addLayout(controlsLayout);
    panelLayout->addWidget(splitter, 1);
    panel->setLayout(panelLayout);

    connect(m_ideasView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &RequirementsWidget::onIdeaSelectionChanged);
    connect(m_ideasSearchEdit, &QLineEdit::textChanged,
            this, &RequirementsWidget::onIdeaSearchChanged);
    connect(m_promoteButton, &QPushButton::clicked,
            this, &RequirementsWidget::onPromoteIdea);
    return panel;
}

void RequirementsWidget::refreshIdeasTab()
{
    m_ideas = IdeaReader::readFromRoots(m_roots);
    m_currentIdeaRow = -1;
    m_ideaModel->setIdeas(m_ideas);
    m_ideaPreview->clear();
    m_promoteButton->setEnabled(false);

    if (m_ideasTabIndex < 0)
        return;
    // No idea files → no tab at all (not an empty tab).
    m_tabs->setTabVisible(m_ideasTabIndex, !m_ideas.isEmpty());
    if (m_ideas.isEmpty() && m_tabs->currentIndex() == m_ideasTabIndex)
        m_tabs->setCurrentIndex(0);
}

void RequirementsWidget::showIdeaPreview(const Idea &idea)
{
    // Reuse the requirement renderer (no second Markdown engine): the fields
    // the requirement header would show are filled from the idea where they
    // exist, and the body is the idea's own .md.
    Requirement asRequirement;
    asRequirement.id = idea.id;
    asRequirement.title = idea.title;
    asRequirement.status = idea.status;
    asRequirement.priority = QStringLiteral("—");
    asRequirement.assignee = QStringLiteral("—");
    asRequirement.date = idea.added;
    asRequirement.repo = QStringLiteral("idea");
    asRequirement.filePath = idea.path;
    asRequirement.rawContent = idea.rawContent;
    RequirementPreview::render(m_ideaPreview, asRequirement, {}, {});
}

void RequirementsWidget::onIdeaSelectionChanged()
{
    const Idea *idea = m_ideaModel->ideaAt(m_ideaProxy->mapToSource(m_ideasView->currentIndex()));
    if (!idea) {
        m_currentIdeaRow = -1;
        m_ideaPreview->clear();
        m_promoteButton->setEnabled(false);
        return;
    }

    m_currentIdeaRow = -1;
    for (int i = 0; i < m_ideas.size(); ++i) {
        if (m_ideas.at(i).path == idea->path) {
            m_currentIdeaRow = i;
            break;
        }
    }
    showIdeaPreview(*idea);
    m_promoteButton->setEnabled(m_currentIdeaRow >= 0);
}

void RequirementsWidget::onIdeaSearchChanged(const QString &text)
{
    m_ideaProxy->setQuery(text);
    if (!m_ideasView->currentIndex().isValid()) {
        m_currentIdeaRow = -1;
        m_ideaPreview->clear();
        m_promoteButton->setEnabled(false);
    }
}

QString RequirementsWidget::rootForIdea(const Idea &idea) const
{
    // A promoted requirement belongs to the repo the idea lives in.
    QString best;
    const QString separator = QDir::separator();
    for (const QString &root : m_roots) {
        if (idea.path.startsWith(root + separator) && root.size() > best.size())
            best = root;
    }
    return best.isEmpty() ? m_baseDir : best;
}

void RequirementsWidget::onPromoteIdea()
{
    if (m_currentIdeaRow < 0 || m_currentIdeaRow >= m_ideas.size())
        return;

    const Idea idea = m_ideas.at(m_currentIdeaRow);
    NewRequirementDialog dialog(rootForIdea(idea), this);
    dialog.setPrefill(idea.title,
                      tr("Изведено от идея %1: %2").arg(idea.id, idea.path),
                      idea.id);

    // Cancel (or closing the dialog) writes nothing — neither the requirement
    // nor the promotion line in the idea.
    if (dialog.exec() != QDialog::Accepted || dialog.createdId().isEmpty())
        return;

    if (!IdeaReader::appendPromotionLine(idea.path, dialog.createdId())) {
        qCWarning(lcFramework) << "RequirementsManager: cannot record promotion in"
                               << idea.path;
    }

    const QString createdPath = dialog.createdFilePath();
    reload();
    for (const Requirement &req : m_requirements) {
        if (req.filePath == createdPath) {
            m_tabs->setCurrentIndex(0);
            navigateToId(req.id);
            break;
        }
    }
}

} // namespace Daqster
