#include "AppToolbar.h"
#include "AppSelectionDialog.h"

#include <QAction>
#include <QIcon>
#include <QDebug>
#include <QMenu>
#include <QToolButton>
#include <QFile>
#include <QDir>
#include <QApplication>
#include <QSettings>
#include <QShowEvent>
#include <QWindow>
#include <QWidget>
#include <QHBoxLayout>
#include "ApplicationsManager.h"
#include "debug.h"

AppToolbar::AppToolbar(QWidget* parent)
    : QToolBar(parent)
    , m_killBtn(nullptr)
    , m_killDropdown(nullptr)
    , m_killMenu(nullptr)
    , m_menuBtn(nullptr)
{
    DEBUG << "AppToolbar constructed";

    setObjectName("DaqsterToolbar");
    setWindowTitle("Daqster");
    setMovable(true);
    setFloatable(true);
    setAllowedAreas(Qt::TopToolBarArea | Qt::BottomToolBarArea |
                     Qt::LeftToolBarArea | Qt::RightToolBarArea);
    setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
   setIconSize(QSize(24, 24));

    // ===== Column 1: Menu (Setup + Quit) =====
    QToolButton* menuBtn = new QToolButton(this);
    menuBtn->setText("Menu");
    menuBtn->setObjectName("menuBtn");
    menuBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_menuBtn = new QMenu();
    m_menuBtn->addAction("Setup", this, SLOT(configureAppSelection()));
    m_menuBtn->addSeparator();
    m_menuBtn->addAction("Quit", this, SLOT(onQuitTriggered()));
    menuBtn->setMenu(m_menuBtn);
    menuBtn->setPopupMode(QToolButton::InstantPopup);
    addWidget(menuBtn);

    addSeparator();

    // ===== Column 2: [Stop All][│▾] split button =====
    QWidget* stopContainer = new QWidget(this);
    QHBoxLayout* stopLayout = new QHBoxLayout(stopContainer);
    stopLayout->setContentsMargins(0, 0, 0, 0);
    stopLayout->setSpacing(0);

    m_killBtn = new QToolButton(stopContainer);
    m_killBtn->setText("Stop All");
    m_killBtn->setToolTip("Kill all running applications");
    m_killBtn->setObjectName("stopAllBtn");
    m_killBtn->setMinimumHeight(28);
    connect(m_killBtn, SIGNAL(clicked(bool)), this, SLOT(onKillAllTriggered()));
    stopLayout->addWidget(m_killBtn);

    m_killDropdown = new QToolButton(stopContainer);
    m_killDropdown->setText("  ▾  ");
    m_killDropdown->setToolTip("Kill individual process");
    m_killDropdown->setObjectName("killDropdownBtn");
    m_killDropdown->setFixedWidth(18);
    m_killDropdown->setMinimumHeight(28);
    m_killMenu = new QMenu();
    QAction* placeholder = m_killMenu->addAction("No running apps");
    placeholder->setEnabled(false);
    m_killDropdown->setMenu(m_killMenu);
    m_killDropdown->setPopupMode(QToolButton::InstantPopup);
    stopLayout->addWidget(m_killDropdown);

    addWidget(stopContainer);

    addSeparator();

    // ===== Column 3: Plugin launch buttons =====
    buildPluginButtons();

    // Connections
    connect(this,
            SIGNAL(PleaseRunApplication(QString, QStringList, QProcess::OpenMode)),
            &ApplicationsManager::Instance(),
            SLOT(StartApplication(QString, QStringList, QProcess::OpenMode)));

    connect(&ApplicationsManager::Instance(),
            SIGNAL(ApplicationEvent(ApplicationsManager::AppHndl_t, ApplicationsManager::AppEvent_t)),
            this,
            SLOT(ApplicationEvent(ApplicationsManager::AppHndl_t, ApplicationsManager::AppEvent_t)));
}

AppToolbar::~AppToolbar() {
    DEBUG << "AppToolbar destroyed";
}

void AppToolbar::showEvent(QShowEvent* e) {
    QToolBar::showEvent(e);
    applyThemeHint();
    if (!e->spontaneous()) {
        move(10, 10);
    }
}

void AppToolbar::applyThemeHint() {
    QSettings settings("Daqster", "Daqster");
    if (settings.value("Theme/Style", "default").toString() == "dark") {
        QWindow* win = windowHandle();
        if (win) {
            win->setProperty("_GTK_THEME_VARIANT", "dark");
            win->setProperty("XDG_CURRENT_DESKTOP", "dark");
        }
    }
}

// ====================== Plugin buttons ======================

void AppToolbar::buildPluginButtons() {
    QList<QAction*> toRemove;
    for (QAction* act : actions()) {
        QString name = act->objectName();
        if (!name.isEmpty() && name.startsWith("__plugin_")) {
            toRemove.append(act);
        }
    }
    for (QAction* act : toRemove) {
        removeAction(act);
        delete act;
    }

    QSettings settings("Daqster", "Daqster");
    settings.beginGroup("AppSelection");

    QList<Daqster::PluginDescription> list = GetAppPluginList();
    foreach (const Daqster::PluginDescription& val, list) {
        QString hash = val.GetProperty(PLUGIN_HASH).toString();
        bool visible = settings.value(hash, true).toBool();
        if (!visible) continue;

        QAction* act = new QAction(this);
        act->setText(val.GetProperty(PLUGIN_NAME).toString());
        act->setObjectName("__plugin_" + hash);
        act->setToolTip(QString("%1\n%2")
            .arg(val.GetProperty(PLUGIN_NAME).toString())
            .arg(val.GetProperty(PLUGIN_DESCRIPTION).toString()));
        if (!val.GetIcon().isNull())
            act->setIcon(val.GetIcon());
        connect(act, SIGNAL(triggered(bool)), this, SLOT(OnActionTrigered()));
        addAction(act);
    }

    settings.endGroup();
}

// ====================== Kill dropdown ======================

void AppToolbar::updateKillMenu() {
    m_killMenu->clear();

    if (m_runningApps.isEmpty()) {
        QAction* ph = m_killMenu->addAction("No running apps");
        ph->setEnabled(false);
        m_killBtn->setEnabled(false);
        m_killDropdown->setEnabled(false);
        return;
    }

    m_killBtn->setEnabled(true);
    m_killDropdown->setEnabled(true);

    QMapIterator<ApplicationsManager::AppHndl_t, QString> it(m_runningApps);
    while (it.hasNext()) {
        it.next();
        QAction* act = m_killMenu->addAction(QString("  x  %1").arg(it.value()));
        act->setData(QVariant::fromValue((uint)it.key()));
        connect(act, SIGNAL(triggered(bool)), this, SLOT(onKillAppTriggered()));
    }
}

// ====================== Slots ======================

void AppToolbar::configureAppSelection() {
    AppSelectionDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        buildPluginButtons();
        applyThemeHint();
    }
}

void AppToolbar::onQuitTriggered() {
    ApplicationsManager::Instance().KillAll();
    qApp->quit();
}

void AppToolbar::onKillAllTriggered() {
    ApplicationsManager::Instance().KillAll();
}

void AppToolbar::onKillAppTriggered() {
    QAction* action = qobject_cast<QAction*>(QObject::sender());
    if (action) {
        ApplicationsManager::AppHndl_t handle = action->data().value<ApplicationsManager::AppHndl_t>();
        ApplicationsManager::Instance().KillApp(handle);
    }
}

void AppToolbar::ApplicationEvent(const ApplicationsManager::AppHndl_t ApHndl,
                                  const ApplicationsManager::AppEvent_t& ev)
{
    switch (ev) {
    case ApplicationsManager::APP_STARTED: {
        ApplicationsManager::AppDescriptor_t Desc;
        if (ApplicationsManager::Instance().GetAppDescryptor(ApHndl, Desc)) {
            m_runningApps[ApHndl] = Desc.Name;
            updateKillMenu();
        }
        break;
    }
    case ApplicationsManager::APP_STOPED: {
        m_runningApps.remove(ApHndl);
        updateKillMenu();
        break;
    }
    default:
        break;
    }
}

bool AppToolbar::GetAppPluginDescription(const QString& Hash, Daqster::PluginDescription& Desc) {
    bool Ret = false;
    QList<Daqster::PluginDescription> list = GetAppPluginList();
    foreach (auto pl, list) {
        if (0 == pl.GetProperty(PLUGIN_HASH).toString().compare(Hash, Qt::CaseInsensitive)) {
            Desc = pl;
            Ret = true;
            break;
        }
    }
    return Ret;
}

QList<Daqster::PluginDescription> AppToolbar::GetAppPluginList() {
    Daqster::PluginFilter Filter;
    Filter.AddFilter(PLUGIN_TYPE, QString("%1").arg(Daqster::PluginDescription::APPLICATION_PLUGIN));
    return Daqster::QPluginManager::instance()->GetPluginList(Filter);
}

void AppToolbar::OnActionTrigered() {
    QAction* sender = qobject_cast<QAction*>(QObject::sender());
    if (!sender) return;

    QString pluginHash = sender->objectName().replace("__plugin_", "");

    QString executablePath;
    QString appImageEnv = qgetenv("APPIMAGE");
    if (!appImageEnv.isEmpty()) {
        QString appImagePath = qApp->applicationDirPath() + "/../AppRun";
        if (QFile::exists(appImagePath))
            executablePath = appImagePath;
    }
    if (executablePath.isEmpty())
        executablePath = qApp->applicationDirPath() + "/Daqster";

    emit PleaseRunApplication(executablePath, QStringList(pluginHash));
}
