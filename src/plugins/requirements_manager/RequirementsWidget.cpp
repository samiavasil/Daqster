#include "RequirementsWidget.h"

#include "RequirementsModel.h"
#include "RequirementsSortProxyModel.h"
#include "RequirementsParser.h"
#include "RequirementsSearchEngine.h"
#include "DependencyGraphWidget.h"
#include "TraceabilityMatrixWidget.h"
#include "NewRequirementDialog.h"
#include "ValidationDialog.h"
#include "HelpDialog.h"
#include "RequirementPreview.h"
#include "LogCategories.h"

#include <QTreeView>
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
#include <QDir>
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
    m_depsAction->setEnabled(false);
    m_editAction->setEnabled(false);

    m_criteriaList = new QListWidget(this);
    m_criteriaList->setVisible(false);

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
    m_actionBar->addAction(m_doneAction);
    m_actionBar->addAction(m_reopenAction);
    m_actionBar->addSeparator();
    m_actionBar->addAction(m_validateAction);
    m_actionBar->addAction(m_helpAction);
    m_actionBar->addSeparator();
    m_actionBar->addAction(m_browseAction);

    fileLayout->addWidget(m_actionBar);

    fileLayout->addWidget(m_validationLabel);
    fileLayout->addWidget(m_preview);
    fileLayout->addWidget(m_editor);
    fileLayout->addWidget(m_criteriaList);

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

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(m_splitter, tr("Requirements"));
    m_tabs->addTab(m_graphWidget, tr("Dependency Graph"));
    m_tabs->addTab(m_matrixWidget, tr("Traceability"));

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
    QVector<RequirementRoot> roots;
    for (const QString &root : m_roots)
        roots.append(RequirementRoot{root});
    m_requirements = RequirementsParser::parseDirectories(roots);

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
    m_graphWidget->setRequirements(m_filtered, m_validationIssues);
    updateValidationStatus();
    refreshActionState();
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

    m_editAction->setEnabled(valid);
    m_depsAction->setEnabled(valid);
    m_doneAction->setEnabled(active);
    m_reopenAction->setEnabled(archived);
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

} // namespace Daqster
