#include "NewRequirementDialog.h"

#include "RequirementsParser.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

namespace Daqster {

NewRequirementDialog::NewRequirementDialog(const QString &baseDir, QWidget *parent)
    : QDialog(parent)
    , m_baseDir(baseDir)
{
    setWindowTitle(tr("New Requirement"));

    m_prefixCombo = new QComboBox(this);
    // Public requirements use the typed scheme REQ-SW-<TYPE>-<NN>
    // (SW-PL plugins, SW-FW framework, SW-APP app, SW-BLD build & tooling);
    // private ones keep the 3-segment REQ-<PREFIX>-<NN> form.
    m_prefixCombo->addItems({QStringLiteral("SW-PL"), QStringLiteral("SW-FW"),
                             QStringLiteral("SW-APP"), QStringLiteral("SW-BLD"),
                             QStringLiteral("PLG"), QStringLiteral("AI"),
                             QStringLiteral("SEC"), QStringLiteral("DOC")});

    m_idLabel = new QLabel(this);

    m_titleEdit = new QLineEdit(this);
    m_titleEdit->setPlaceholderText(tr("Short descriptive title..."));

    m_statusCombo = new QComboBox(this);
    m_statusCombo->addItems({QStringLiteral("ACTIVE"), QStringLiteral("DONE"),
                             QStringLiteral("CANCELLED")});

    m_priorityCombo = new QComboBox(this);
    m_priorityCombo->addItems({QStringLiteral("High"), QStringLiteral("Medium"),
                               QStringLiteral("Low")});

    m_assigneeCombo = new QComboBox(this);
    m_assigneeCombo->addItems({QStringLiteral("PM"), QStringLiteral("Architect"),
                               QStringLiteral("Implementation"), QStringLiteral("QA")});

    m_parentCombo = new QComboBox(this);
    m_parentCombo->addItem(QStringLiteral("—"));
    const QVector<Requirement> existing =
        RequirementsParser::parseDirectory(m_baseDir);
    for (const Requirement &req : existing)
        m_parentCombo->addItem(req.id);
    m_parentCombo->setCurrentIndex(0);

    m_dependenciesList = new QListWidget(this);
    for (const Requirement &req : existing) {
        QListWidgetItem *item = new QListWidgetItem(req.id, m_dependenciesList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }

    m_descriptionEdit = new QPlainTextEdit(this);
    m_descriptionEdit->setPlaceholderText(tr("Free-text description (## Описание)..."));

    m_criteriaList = new QListWidget(this);
    m_criteriaList->setSelectionMode(QAbstractItemView::SingleSelection);

    QPushButton *addCriterion = new QPushButton(tr("Add"), this);
    QPushButton *removeCriterion = new QPushButton(tr("Remove"), this);

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText(tr("Create Requirement"));

    QFormLayout *metaLayout = new QFormLayout;
    metaLayout->addRow(tr("Prefix:"), m_prefixCombo);
    metaLayout->addRow(tr("ID:"), m_idLabel);
    metaLayout->addRow(tr("Title:"), m_titleEdit);
    metaLayout->addRow(tr("Status:"), m_statusCombo);
    metaLayout->addRow(tr("Priority:"), m_priorityCombo);
    metaLayout->addRow(tr("Assignee:"), m_assigneeCombo);
    metaLayout->addRow(tr("Parent:"), m_parentCombo);

    QHBoxLayout *criterionButtonLayout = new QHBoxLayout;
    criterionButtonLayout->addWidget(addCriterion);
    criterionButtonLayout->addWidget(removeCriterion);
    criterionButtonLayout->addStretch();

    QVBoxLayout *criteriaLayout = new QVBoxLayout;
    criteriaLayout->addWidget(m_criteriaList);
    criteriaLayout->addLayout(criterionButtonLayout);

    QGroupBox *criteriaBox = new QGroupBox(tr("Acceptance Criteria"), this);
    criteriaBox->setLayout(criteriaLayout);

    QGroupBox *dependenciesBox = new QGroupBox(tr("Зависи от (Dependencies)"), this);
    QVBoxLayout *dependenciesLayout = new QVBoxLayout;
    dependenciesLayout->addWidget(m_dependenciesList);
    dependenciesBox->setLayout(dependenciesLayout);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(metaLayout);
    mainLayout->addWidget(new QLabel(tr("Описание (Description):"), this));
    mainLayout->addWidget(m_descriptionEdit);
    mainLayout->addWidget(dependenciesBox);
    mainLayout->addWidget(criteriaBox);
    mainLayout->addWidget(buttons);
    setLayout(mainLayout);

    connect(m_prefixCombo, &QComboBox::currentTextChanged,
            this, &NewRequirementDialog::onPrefixChanged);
    connect(addCriterion, &QPushButton::clicked,
            this, &NewRequirementDialog::onAddCriterion);
    connect(removeCriterion, &QPushButton::clicked,
            this, &NewRequirementDialog::onRemoveCriterion);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        QString content;
        QString id;
        QString slug;
        buildMarkdown(content, id, slug);
        if (writeFile(content, id, slug))
            accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    onPrefixChanged();
    resize(560, 640);
}

QString NewRequirementDialog::createdFilePath() const
{
    return m_createdFilePath;
}

void NewRequirementDialog::onPrefixChanged()
{
    m_idLabel->setText(RequirementsParser::generateNextId(m_baseDir,
                                                          m_prefixCombo->currentText()));
}

void NewRequirementDialog::onAddCriterion()
{
    QListWidgetItem *item = new QListWidgetItem(QString(), m_criteriaList);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    m_criteriaList->setCurrentItem(item);
    m_criteriaList->editItem(item);
}

void NewRequirementDialog::onRemoveCriterion()
{
    QListWidgetItem *item = m_criteriaList->currentItem();
    if (item)
        delete item;
}

QString NewRequirementDialog::activeDirectory() const
{
    return RequirementsParser::activeDirectory(m_baseDir);
}

QString NewRequirementDialog::slugFor(const QString &title) const
{
    QString slug = title.toLower();
    slug.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")),
                 QStringLiteral("-"));
    while (slug.startsWith(QLatin1Char('-')))
        slug.remove(0, 1);
    while (slug.endsWith(QLatin1Char('-')))
        slug.chop(1);
    return slug.isEmpty() ? QStringLiteral("requirement") : slug;
}

void NewRequirementDialog::buildMarkdown(QString &outContent, QString &outId,
                                         QString &outSlug) const
{
    outId = RequirementsParser::generateNextId(m_baseDir, m_prefixCombo->currentText());
    const QString title = m_titleEdit->text().trimmed();
    outSlug = slugFor(title);

    QStringList dependencies;
    for (int i = 0; i < m_dependenciesList->count(); ++i) {
        QListWidgetItem *item = m_dependenciesList->item(i);
        if (item->checkState() == Qt::Checked)
            dependencies.append(item->text());
    }

    QStringList criteria;
    for (int i = 0; i < m_criteriaList->count(); ++i) {
        const QString text = m_criteriaList->item(i)->text().trimmed();
        if (!text.isEmpty())
            criteria.append(text);
    }

    const QString parent = m_parentCombo->currentText();
    const QString parentLine = (parent == QStringLiteral("—"))
        ? QStringLiteral("- **Родител:** —")
        : QStringLiteral("- **Родител:** %1").arg(parent);
    const QString depsLine = dependencies.isEmpty()
        ? QStringLiteral("- **Зависи от:** —")
        : QStringLiteral("- **Зависи от:** %1").arg(dependencies.join(QStringLiteral(", ")));

    QString content;
    content += QStringLiteral("# %1: %2\n\n").arg(outId, title);
    content += QStringLiteral("- **Статус:** %1\n").arg(m_statusCombo->currentText());
    content += QStringLiteral("- **Приоритет:** %1\n").arg(m_priorityCombo->currentText());
    content += QStringLiteral("- **Отговорник (роля):** %1\n").arg(m_assigneeCombo->currentText());
    content += QStringLiteral("- **Дата:** %1\n").arg(QDate::currentDate().toString(Qt::ISODate));
    content += parentLine + QStringLiteral("\n");
    content += depsLine + QStringLiteral("\n\n");
    content += QStringLiteral("## Описание\n\n");
    content += m_descriptionEdit->toPlainText().trimmed();
    content += QStringLiteral("\n\n## Acceptance Criteria\n\n");
    for (const QString &criterion : criteria)
        content += QStringLiteral("- [ ] %1\n").arg(criterion);
    content += QStringLiteral("\n## Проследимост\n\n");
    content += QStringLiteral("- **Коммити:** —\n");
    content += QStringLiteral("- **Код:** `src/plugins/requirements_manager/`\n");
    content += QStringLiteral("- **Документация:** `docs/Architecture/plugins/`\n");
    content += QStringLiteral("- **Тестове:** Qt5 + Qt6 builds\n");

    outContent = content;
}

bool NewRequirementDialog::writeFile(const QString &content, const QString &id,
                                     const QString &slug)
{
    QDir().mkpath(activeDirectory());
    const QString filePath =
        QDir(activeDirectory()).filePath(QStringLiteral("%1-%2.md").arg(id, slug));

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(content.toUtf8());
    file.close();
    m_createdFilePath = filePath;
    return true;
}

} // namespace Daqster
