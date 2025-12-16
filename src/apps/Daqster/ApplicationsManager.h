#ifndef APPLICATIONSMANAGER_H
#define APPLICATIONSMANAGER_H
#include<QObject>
#include<QMap>
#include<QString>
#include<QProcess>
#include"main.h"

class ApplicationsManager:public QObject
{
    Q_OBJECT
public:
    typedef enum{
        APP_NA,
        APP_STARTED,
        APP_STOPED
    }AppEvent_t;

    typedef struct{
        QString            Name;
        QStringList        Arguments;
        QProcess::OpenMode Mode;
    }AppDescriptor_t;

    typedef uint32_t AppHndl_t;
    static ApplicationsManager& Instance();
    ~ApplicationsManager();
    bool GetAppDescryptor(const AppHndl_t& Hndl, AppDescriptor_t& Desc );
    void SetHeadlessMode(bool enabled);
public slots:
    void StartApplication(const QString& Name, const QStringList & Arguments, QProcess::OpenMode Mode = QProcess::ReadWrite );

    void KillAll();
signals:
    void ApplicationEvent(const ApplicationsManager::AppHndl_t &AppHnd, const ApplicationsManager::AppEvent_t &ev);
protected slots:
    void AppFinished(int exitCode, QProcess::ExitStatus exitStatus);
private:
    ApplicationsManager();
    AppHndl_t nextHndl;
    QMap<AppHndl_t,QProcess*>  m_ProcessMap;
    QMap<AppHndl_t,AppDescriptor_t>  m_ProcessDescs;
    static ApplicationsManager* m_Manager;
    bool m_headlessMode;
};

#endif // APPLICATIONSMANAGER_H
