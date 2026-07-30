#include "DebugConsoleWidget.h"

#include "include/LogManager.h"
#include "include/LogCategories.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMap>
#include <QList>
#include <QPair>
#include <QScrollArea>
#include <QFrame>
#include <QComboBox>

namespace Daqster {

struct DebugConsoleWidget::Private {
    QCheckBox *enableAllCheckBox = nullptr;
    QMap<QString, QCheckBox *> categoryCheckBoxes;
    QCheckBox *consoleEnableCheckBox = nullptr;
    QComboBox *levelCombo = nullptr;
    QCheckBox *fileCheckBox = nullptr;
    QLineEdit *filePathEdit = nullptr;
    QPushButton *browseButton = nullptr;
    QPushButton *resetButton = nullptr;
};

DebugConsoleWidget::DebugConsoleWidget(QWidget *parent)
    : QWidget(parent)
    , d(new Private)
{
    setupUi();
    loadCurrentState();
}

DebugConsoleWidget::~DebugConsoleWidget()
{
    delete d;
}

void DebugConsoleWidget::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    setMinimumSize(450, 500);
    setWindowTitle(tr("Debug Console Settings"));
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // ── Master toggle ──────────────────────────────────
    d->enableAllCheckBox = new QCheckBox(tr("Enable Debug Logging (all categories)"), this);
    mainLayout->addWidget(d->enableAllCheckBox);

    // ── Scroll area for categories ─────────────────────
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *scrollWidget = new QWidget();
    auto *scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(8);

    // ── Category groups (built dynamically from Log::allCategories()) ──
    QVector<Log::CategoryInfo> allCats = Log::allCategories();

    // Group by top-level prefix (e.g., "daqster.framework", "daqster.plugin")
    // Key: group prefix (e.g., "daqster.framework") -> list of (name, description)
    QMap<QString, QList<QPair<QString, QString>>> groups;
    for (const auto &cat : allCats) {
        QString name = QString::fromUtf8(cat.name);
        QString desc = QString::fromUtf8(cat.description);

        // Derive group name from prefix (first 2 segments)
        // e.g., "daqster.framework.registry" -> "Framework"
        //        "daqster.app"                 -> "Application"
        //        "daqster.plugin.aistudio"     -> "Plugins"
        QStringList parts = name.split('.');
        QString groupKey;
        if (parts.size() >= 3) {
            groupKey = parts.mid(0, 2).join('.');
        } else if (parts.size() >= 2) {
            groupKey = parts.mid(0, 2).join('.');
        } else {
            groupKey = name;
        }
        groups[groupKey].append({name, desc});
    }

    // Create group boxes in order
    // Sort by group key to ensure consistent order
    QStringList sortedGroups = groups.keys();
    sortedGroups.sort();

    for (const QString &groupKey : sortedGroups) {
        // Human-readable group title
        QStringList keyParts = groupKey.split('.');
        QString groupTitle;
        if (keyParts.size() >= 2) {
            // Capitalize second part: "daqster.framework" -> "Framework"
            groupTitle = keyParts[1];
            groupTitle[0] = groupTitle[0].toUpper();
        } else {
            groupTitle = groupKey;
        }

        auto *groupBox = new QGroupBox(groupTitle, scrollWidget);
        auto *groupLayout = new QVBoxLayout(groupBox);
        groupLayout->setContentsMargins(12, 8, 12, 8);
        groupLayout->setSpacing(4);

        const auto &catList = groups[groupKey];
        for (const auto &cat : catList) {
            auto *cb = new QCheckBox(cat.second, groupBox);  // Use description as label
            cb->setToolTip(cat.first);  // Full category name as tooltip
            d->categoryCheckBoxes.insert(cat.first, cb);
            groupLayout->addWidget(cb);
        }

        scrollLayout->addWidget(groupBox);
    }

    scrollLayout->addStretch();
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);

    // ── Console output section ──────────────────────────
    auto *consoleGroup = new QGroupBox(tr("Console Output"), this);
    auto *consoleLayout = new QVBoxLayout(consoleGroup);
    consoleLayout->setContentsMargins(12, 8, 12, 8);

    auto *consoleEnableRow = new QHBoxLayout();
    auto *consoleEnableLabel = new QLabel(tr("Enable Console Logging:"));
    d->consoleEnableCheckBox = new QCheckBox();
    consoleEnableRow->addWidget(consoleEnableLabel);
    consoleEnableRow->addStretch();
    consoleEnableRow->addWidget(d->consoleEnableCheckBox);
    consoleLayout->addLayout(consoleEnableRow);

    auto *levelRow = new QHBoxLayout();
    auto *levelLabel = new QLabel(tr("Minimum Log Level:"));
    d->levelCombo = new QComboBox();
    d->levelCombo->addItem(tr("Debug"), static_cast<int>(LogLevel::Debug));
    d->levelCombo->addItem(tr("Info"), static_cast<int>(LogLevel::Info));
    d->levelCombo->addItem(tr("Warning"), static_cast<int>(LogLevel::Warning));
    d->levelCombo->addItem(tr("Critical"), static_cast<int>(LogLevel::Critical));
    d->levelCombo->addItem(tr("Fatal"), static_cast<int>(LogLevel::Fatal));
    d->levelCombo->setCurrentIndex(2);  // Default: Warning
    d->levelCombo->setEnabled(false);   // Disabled until console is enabled
    levelRow->addWidget(levelLabel);
    levelRow->addWidget(d->levelCombo, 1);
    consoleLayout->addLayout(levelRow);

    mainLayout->addWidget(consoleGroup);

    // ── File output section ─────────────────────────────
    auto *outputGroup = new QGroupBox(tr("File Output"), this);
    auto *outputLayout = new QVBoxLayout(outputGroup);
    outputLayout->setContentsMargins(12, 8, 12, 8);
    outputLayout->setSpacing(8);

    auto *fileRow = new QHBoxLayout();
    d->fileCheckBox = new QCheckBox(tr("Log to file:"), outputGroup);
    d->filePathEdit = new QLineEdit(outputGroup);
    d->filePathEdit->setPlaceholderText(tr("Path to log file"));
    d->browseButton = new QPushButton(tr("Browse..."), outputGroup);

    fileRow->addWidget(d->fileCheckBox);
    fileRow->addWidget(d->filePathEdit, 1);
    fileRow->addWidget(d->browseButton);
    outputLayout->addLayout(fileRow);

    mainLayout->addWidget(outputGroup);

    // ── Reset button ───────────────────────────────────
    d->resetButton = new QPushButton(tr("Reset to Defaults"), this);
    mainLayout->addWidget(d->resetButton);

    // ── Connections ────────────────────────────────────
    connect(d->enableAllCheckBox, &QCheckBox::toggled,
            this, &DebugConsoleWidget::onEnableAllToggled);

    for (auto it = d->categoryCheckBoxes.constBegin();
         it != d->categoryCheckBoxes.constEnd(); ++it) {
        const QString category = it.key();
        connect(it.value(), &QCheckBox::toggled,
                this, [this, category](bool checked) {
                    onCategoryToggled(category, checked);
                });
    }

    connect(d->consoleEnableCheckBox, &QCheckBox::toggled, [this](bool checked) {
        d->levelCombo->setEnabled(checked);
        LogManager::instance()->setConsoleEnabled(checked);
    });

    connect(d->levelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        LogLevel level = static_cast<LogLevel>(d->levelCombo->itemData(index).toInt());
        LogManager::instance()->setConsoleLogLevel(level);
    });

    connect(d->fileCheckBox, &QCheckBox::toggled,
            this, &DebugConsoleWidget::onFileToggled);

    connect(d->browseButton, &QPushButton::clicked,
            this, &DebugConsoleWidget::onBrowseClicked);

    connect(d->resetButton, &QPushButton::clicked,
            this, &DebugConsoleWidget::onResetClicked);
}

void DebugConsoleWidget::loadCurrentState()
{
    LogManager *lm = LogManager::instance();

    // Master toggle: all categories enabled?
    bool allEnabled = true;
    for (auto it = d->categoryCheckBoxes.constBegin();
         it != d->categoryCheckBoxes.constEnd(); ++it) {
        if (!lm->isCategoryEnabled(it.key())) {
            allEnabled = false;
        }
    }
    d->enableAllCheckBox->setChecked(allEnabled);

    // Individual categories
    for (auto it = d->categoryCheckBoxes.constBegin();
         it != d->categoryCheckBoxes.constEnd(); ++it) {
        it.value()->setChecked(lm->isCategoryEnabled(it.key()));
    }

    // Console enabled
    d->consoleEnableCheckBox->setChecked(lm->isConsoleEnabled());
    d->levelCombo->setEnabled(lm->isConsoleEnabled());

    // Console log level
    LogLevel currentLevel = lm->consoleLogLevel();
    d->levelCombo->setCurrentIndex(static_cast<int>(currentLevel));

    // File output
    QString filePath = lm->logFilePath();
    d->fileCheckBox->setChecked(!filePath.isEmpty());
    d->filePathEdit->setText(filePath);
    d->filePathEdit->setEnabled(!filePath.isEmpty());
    d->browseButton->setEnabled(!filePath.isEmpty());
}

void DebugConsoleWidget::onEnableAllToggled(bool checked)
{
    LogManager *lm = LogManager::instance();
    if (checked) {
        lm->enableAll();
    } else {
        lm->disableAll();
    }

    // Update individual checkboxes without triggering slots
    for (auto it = d->categoryCheckBoxes.constBegin();
         it != d->categoryCheckBoxes.constEnd(); ++it) {
        it.value()->blockSignals(true);
        it.value()->setChecked(checked);
        it.value()->blockSignals(false);
    }
}

void DebugConsoleWidget::onCategoryToggled(const QString &category, bool checked)
{
    LogManager *lm = LogManager::instance();
    if (checked) {
        lm->enableCategory(category);
    } else {
        lm->disableCategory(category);
    }

    // Update master toggle
    bool allEnabled = true;
    for (auto it = d->categoryCheckBoxes.constBegin();
         it != d->categoryCheckBoxes.constEnd(); ++it) {
        if (!it.value()->isChecked()) {
            allEnabled = false;
            break;
        }
    }
    d->enableAllCheckBox->blockSignals(true);
    d->enableAllCheckBox->setChecked(allEnabled);
    d->enableAllCheckBox->blockSignals(false);
}

void DebugConsoleWidget::onFileToggled(bool checked)
{
    d->filePathEdit->setEnabled(checked);
    d->browseButton->setEnabled(checked);

    if (!checked) {
        LogManager::instance()->setLogFile(QString());
    } else if (!d->filePathEdit->text().isEmpty()) {
        LogManager::instance()->setLogFile(d->filePathEdit->text());
    }
}

void DebugConsoleWidget::onBrowseClicked()
{
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Select Log File"),
        d->filePathEdit->text(),
        tr("Log files (*.log);;All files (*)")
    );

    if (!filePath.isEmpty()) {
        d->filePathEdit->setText(filePath);
        LogManager::instance()->setLogFile(filePath);
    }
}

void DebugConsoleWidget::onResetClicked()
{
    LogManager *lm = LogManager::instance();
    lm->disableAll();
    lm->setConsoleEnabled(false);  // OFF by default
    lm->setConsoleLogLevel(LogLevel::Warning);
    lm->setLogFile(QString());
    loadCurrentState();
}

} // namespace Daqster
