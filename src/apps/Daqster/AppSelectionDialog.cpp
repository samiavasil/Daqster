#include "AppSelectionDialog.h"

#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QSettings>
#include <QLabel>
#include <QComboBox>
#include <QFile>
#include <QApplication>
#include <QWindow>
#include <QShowEvent>

#include "QPluginManager.h"

AppSelectionDialog::AppSelectionDialog(QWidget *parent)
    : QDialog(parent)
    , m_tree(nullptr)
    , m_themeCombo(nullptr)
    , m_settings("Daqster", "Daqster")
{
    setWindowTitle("Settings");
    setMinimumSize(560, 460);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // === Appearance section ===
    QLabel* appearanceTitle = new QLabel("Appearance", this);
    appearanceTitle->setStyleSheet("font-size: 15px; font-weight: 700;");
    mainLayout->addWidget(appearanceTitle);

    QHBoxLayout* themeLayout = new QHBoxLayout();
    QLabel* themeLabel = new QLabel("Theme:", this);
    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItem("System (default)", "default");
    m_themeCombo->addItem("Dark (modern)", "dark");

    QString currentTheme = m_settings.value("Theme/Style", "default").toString();
    int idx = (currentTheme == "dark") ? 1 : 0;
    m_themeCombo->setCurrentIndex(idx);

    themeLayout->addWidget(themeLabel);
    themeLayout->addWidget(m_themeCombo);
    themeLayout->addStretch();
    mainLayout->addLayout(themeLayout);

    // Separator
    QFrame* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(sep);

    // === Plugins section ===
    QLabel* pluginsTitle = new QLabel("Application Plugins", this);
    pluginsTitle->setStyleSheet("font-size: 15px; font-weight: 700;");
    mainLayout->addWidget(pluginsTitle);

    QLabel* subtitle = new QLabel("Choose which plugins appear in the sidebar:", this);
    subtitle->setStyleSheet("color: #8b949e; font-size: 12px;");
    mainLayout->addWidget(subtitle);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({"Name", "Version", "Location"});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    mainLayout->addWidget(m_tree);

    // === Buttons ===
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    QPushButton* btnAll = new QPushButton("Select All", this);
    QPushButton* btnNone = new QPushButton("Deselect All", this);
    QPushButton* btnOk = new QPushButton("  Apply  ", this);
    QPushButton* btnCancel = new QPushButton("Cancel", this);

    btnOk->setObjectName("primaryButton");

    btnLayout->addWidget(btnAll);
    btnLayout->addWidget(btnNone);
    btnLayout->addStretch();
    btnLayout->addWidget(btnOk);
    btnLayout->addWidget(btnCancel);
    mainLayout->addLayout(btnLayout);

    connect(btnAll,    &QPushButton::clicked, this, &AppSelectionDialog::SelectAll);
    connect(btnNone,   &QPushButton::clicked, this, &AppSelectionDialog::DeselectAll);
    connect(btnOk,     &QPushButton::clicked, this, [this]() { SaveState(); accept(); });
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    LoadPlugins();
    RestoreState();
}

AppSelectionDialog::~AppSelectionDialog() {}

void AppSelectionDialog::showEvent(QShowEvent* e) {
    QDialog::showEvent(e);
    // Tell Linux WM to use dark title bar decorations
    if (m_settings.value("Theme/Style", "default").toString() == "dark") {
        QWindow* win = windowHandle();
        if (win) {
            win->setProperty("_GTK_THEME_VARIANT", "dark");
            win->setProperty("XDG_CURRENT_DESKTOP", "dark");
        }
    }
}

void AppSelectionDialog::LoadPlugins()
{
    m_tree->clear();

    Daqster::PluginFilter filter;
    filter.AddFilter(PLUGIN_TYPE,
                     QString("%1").arg(Daqster::PluginDescription::APPLICATION_PLUGIN));

    QList<Daqster::PluginDescription> list =
        Daqster::QPluginManager::instance()->GetPluginList(filter);

    foreach (const Daqster::PluginDescription& desc, list) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_tree);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setText(0, desc.GetProperty(PLUGIN_NAME).toString());
        item->setText(1, desc.GetProperty(PLUGIN_VERSION).toString());
        item->setText(2, desc.GetProperty(PLUGIN_LOCATION).toString());
        item->setData(0, Qt::UserRole, desc.GetProperty(PLUGIN_NAME).toString());
        item->setCheckState(0, Qt::Checked);
    }
}

bool AppSelectionDialog::IsPluginVisible(const QString& pluginName) const
{
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_tree->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toString() == pluginName) {
            return item->checkState(0) == Qt::Checked;
        }
    }
    return true;
}

void AppSelectionDialog::SaveState()
{
    // Save theme
    QString theme = m_themeCombo->currentData().toString();
    m_settings.setValue("Theme/Style", theme);

    // Apply theme immediately
    if (theme == "dark") {
        QFile styleFile(":/toolbar/icons/StyleFile");
        if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
            qApp->setStyleSheet(styleFile.readAll());
            styleFile.close();
        }
    } else {
        qApp->setStyleSheet("");
    }

    // Save plugin visibility. Keyed by NAME so the user's "hidden" choice
    // survives rebuilds (a hash changes on every rebuild, a name does not).
    m_settings.beginGroup("AppSelection");
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_tree->topLevelItem(i);
        QString name = item->data(0, Qt::UserRole).toString();
        bool visible = (item->checkState(0) == Qt::Checked);
        m_settings.setValue(name, visible);
    }
    m_settings.endGroup();
    m_settings.sync();
}

void AppSelectionDialog::RestoreState()
{
    m_settings.beginGroup("AppSelection");
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_tree->topLevelItem(i);
        QString name = item->data(0, Qt::UserRole).toString();
        if (m_settings.contains(name)) {
            bool visible = m_settings.value(name, true).toBool();
            item->setCheckState(0, visible ? Qt::Checked : Qt::Unchecked);
        }
    }
    m_settings.endGroup();
}

void AppSelectionDialog::SelectAll()
{
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        m_tree->topLevelItem(i)->setCheckState(0, Qt::Checked);
}

void AppSelectionDialog::DeselectAll()
{
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        m_tree->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
}
