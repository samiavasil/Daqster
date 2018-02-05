#include "ApplicationsManager.h"
#include<QDebug>

ApplicationsManager* ApplicationsManager::m_Manager;

ApplicationsManager::ApplicationsManager():QObject(NULL),nextHndl(0)
{

}

ApplicationsManager::~ApplicationsManager()
{
    foreach (auto iter , m_ProcessMap) {
        iter->close();
        iter++;
    }
}

ApplicationsManager &ApplicationsManager::Instance()
{
    if( NULL == m_Manager ){
        m_Manager = new ApplicationsManager();
    }
    return *m_Manager;
}

void ApplicationsManager::StartApplication(const QString &Name , const QStringList &Arguments, QProcess::OpenMode Mode)
{
    QProcess* newProc =  new QProcess();
    AppDescriptor_t Desc = { Name, Arguments, Mode };
    qDebug() << "App: "<< Name << "Args: " << Arguments;
    newProc->start(Name , Arguments,  Mode );
    m_ProcessMap[nextHndl]   =  newProc;
    m_ProcessDescs[nextHndl] =  Desc;
    emit ApplicationEvent(nextHndl,APP_STARTED);
    nextHndl++;
}

 bool ApplicationsManager::GetAppDescryptor(const AppHndl_t &Hndl , AppDescriptor_t &Desc){
     bool ret = false;
     if( m_ProcessDescs.contains(nextHndl) ){
         Desc = m_ProcessDescs[Hndl];
         ret = true;
     }
     return ret;
 }
