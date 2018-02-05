#ifndef APPTOOLBAR_H
#define APPTOOLBAR_H

#include<QToolBar>
#include<QProcess>
#include"ApplicationsManager.h"
#include"QPluginManager.h"

class QMenu;

class AppToolbar : public QToolBar
{
    Q_OBJECT
public:
    explicit AppToolbar(QWidget *parent = 0);
    ~AppToolbar();
public slots:

private slots:
    void OnActionTrigered();
    void ApplicationEvent(const ApplicationsManager::AppHndl_t ApHndl, const ApplicationsManager::AppEvent_t& ev);
signals:
    void PleaseRunApplication(const QString &Name , const QStringList &Arguments, QProcess::OpenMode Mode = QProcess::ReadWrite);
protected:
    QList<Daqster::PluginDescription> GetAppPluginList();
    bool GetAppPluginDescription(const QString &Name, Daqster::PluginDescription& Desc);
private:
    QMenu* m_AppMenu;
};

#endif // APPTOOLBAR_H
