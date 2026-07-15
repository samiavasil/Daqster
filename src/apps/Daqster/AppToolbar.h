#ifndef APPTOOLBAR_H
#define APPTOOLBAR_H

#include<QToolBar>
#include<QProcess>
#include<QList>
#include<QMap>
#include"ApplicationsManager.h"
#include"QPluginManager.h"

class QMenu;
class QAction;
class QToolButton;

class AppToolbar : public QToolBar
{
    Q_OBJECT
public:
    explicit AppToolbar(QWidget *parent = 0);
    ~AppToolbar();

protected:
    void showEvent(QShowEvent *e) override;

private slots:
    void OnActionTrigered();
    void onQuitTriggered();
    void onKillAllTriggered();
    void onKillAppTriggered();
    void configureAppSelection();
    void updateKillMenu();
    void ApplicationEvent(const ApplicationsManager::AppHndl_t ApHndl, const ApplicationsManager::AppEvent_t& ev);

signals:
    void PleaseRunApplication(const QString &Name , const QStringList &Arguments, QProcess::OpenMode Mode = QProcess::ReadWrite);

protected:
    QList<Daqster::PluginDescription> GetAppPluginList();
    bool GetAppPluginDescription(const QString &Hash, Daqster::PluginDescription& Desc);
    void buildPluginButtons();
    void applyThemeHint();

private:
    QToolButton* m_killBtn;
    QToolButton* m_killDropdown;
    QMenu*       m_killMenu;
    QMenu*       m_menuBtn;
    QMap<ApplicationsManager::AppHndl_t, QString> m_runningApps;
};

#endif // APPTOOLBAR_H
