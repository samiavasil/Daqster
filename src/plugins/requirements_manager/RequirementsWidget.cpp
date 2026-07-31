#include "RequirementsWidget.h"

#include "RequirementsModel.h"
#include "RequirementsParser.h"
#include "LogCategories.h"

#include <QTreeView>
#include <QTextBrowser>
#include <QPlainTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QItemSelectionModel>
#include <QListWidgetItem>

namespace Daqster {

RequirementsWidget::RequirementsWidget(QWidget *parent)
    : QWidget(parent)
    , m_currentIndex(-1)
{
    m_model = new RequirementsModel(this);

    m_treeView = new QTreeView(this);
    m_treeView->setModel(m_model);
    m_treeView->setRootIsDecorated(true);
    m_treeView->expandAll();

    m_preview = new QTextBrowser(this);
    m_preview->setReadOnly(true);

    m_editor = new QPlainTextEdit(this);
    m_editor->setVisible(false);

    m_fileLabel = new QLabel(this);

    m_editButton = new QPushButton(tr("Edit"), this);
    m_saveButton = new QPushButton(tr("Save"), this);
    m_cancelButton = new QPushButton(tr("Cancel"), this);
    m_browseButton = new QPushButton(tr("Open requirements folder..."), this);

    m_saveButton->setEnabled(false);
    m_cancelButton->setEnabled(false);

    m_criteriaList = new QListWidget(this);
    m_criteriaList->setVisible(false);

    QVBoxLayout *fileLayout = new QVBoxLayout;
    fileLayout->addWidget(m_fileLabel);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(m_editButton);
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_browseButton);
    fileLayout->addLayout(buttonLayout);

    fileLayout->addWidget(m_preview);
    fileLayout->addWidget(m_editor);
    fileLayout->addWidget(m_criteriaList);

    QWidget *detailsPanel = new QWidget(this);
    detailsPanel->setLayout(fileLayout);

    m_splitter = new QSplitter(this);
    m_splitter->addWidget(m_treeView);
    m_splitter->addWidget(detailsPanel);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 3);
    m_splitter->setChildrenCollapsible(false);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_splitter);
    setLayout(mainLayout);

    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &RequirementsWidget::onSelectionChanged);
    connect(m_editButton, &QPushButton::clicked, this,
            [this]() { onEditToggled(!m_editor->isVisible()); });
    connect(m_saveButton, &QPushButton::clicked, this, &RequirementsWidget::onSave);
    connect(m_cancelButton, &QPushButton::clicked, this, &RequirementsWidget::onCancel);
    connect(m_browseButton, &QPushButton::clicked, this, &RequirementsWidget::onBrowseDirectory);
    connect(m_criteriaList, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        const int index = item->data(Qt::UserRole).toInt();
        onCriterionToggled(index, item->checkState() == Qt::Checked);
    });
}

RequirementsWidget::~RequirementsWidget() = default;

void RequirementsWidget::openDirectory(const QString &baseDir)
{
    m_baseDir = baseDir;
    reload();
}

QString RequirementsWidget::baseDirectory() const
{
    return m_baseDir;
}

void RequirementsWidget::reload()
{
    m_requirements = RequirementsParser::parseDirectory(m_baseDir);
    m_model->setRequirements(m_requirements);
    m_currentIndex = -1;
    m_preview->clear();
    m_editor->clear();
    m_fileLabel->clear();
    m_criteriaList->clear();
    m_treeView->expandAll();
}

void RequirementsWidget::onSelectionChanged()
{
    const QModelIndex index = m_treeView->currentIndex();
    const Requirement *req = m_model->requirementAt(index);
    if (!req) {
        m_currentIndex = -1;
        m_preview->clear();
        m_editor->clear();
        m_fileLabel->clear();
        m_criteriaList->clear();
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
        m_saveButton->setEnabled(true);
        m_cancelButton->setEnabled(true);
        m_editButton->setText(tr("Preview"));
    } else {
        showPreview();
        m_editButton->setText(tr("Edit"));
        m_saveButton->setEnabled(false);
        m_cancelButton->setEnabled(false);
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
    m_editButton->setText(tr("Edit"));
    m_saveButton->setEnabled(false);
    m_cancelButton->setEnabled(false);
}

void RequirementsWidget::onCancel()
{
    if (m_currentIndex < 0)
        return;
    showPreview();
    m_editButton->setText(tr("Edit"));
    m_saveButton->setEnabled(false);
    m_cancelButton->setEnabled(false);
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
        this, tr("Select requirements directory"), m_baseDir);
    if (dir.isEmpty())
        return;
    openDirectory(dir);
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
    QString text;
    text += QStringLiteral("<h2>%1: %2</h2>").arg(req.id.toHtmlEscaped(), req.title.toHtmlEscaped());
    text += QStringLiteral("<p><b>Status:</b> %1 &nbsp; <b>Priority:</b> %2 &nbsp; <b>Assignee:</b> %3</p>")
                .arg(req.status.toHtmlEscaped(), req.priority.toHtmlEscaped(), req.assignee.toHtmlEscaped());
    text += QStringLiteral("<p>%1</p>").arg(req.description.toHtmlEscaped().replace(QStringLiteral("\n"),
                                                                                    QStringLiteral("<br/>")));

    m_preview->setHtml(text);
}

} // namespace Daqster
