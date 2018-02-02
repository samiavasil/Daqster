#include "ApplicationsManager.h"

ApplicationsManager* ApplicationsManager::m_Manager;

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
    newProc->start(Name , Arguments,  Mode );
    m_ProcessList.append( newProc );
}

ApplicationsManager::ApplicationsManager():QObject(NULL)
{

}

