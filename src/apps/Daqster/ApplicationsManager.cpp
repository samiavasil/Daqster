#include "ApplicationsManager.h"
#include<QDebug>

ApplicationsManager* ApplicationsManager::m_Manager;

ApplicationsManager::ApplicationsManager():QObject(nullptr),nextHndl(0)
{

}

ApplicationsManager::~ApplicationsManager()
{
    KillAll();
}

ApplicationsManager &ApplicationsManager::Instance()
{
    if( nullptr == m_Manager ){
        m_Manager = new ApplicationsManager();
    }
    return *m_Manager;
}

void ApplicationsManager::KillAll()
{
    blockSignals(true);
    auto iter = m_ProcessMap.begin();
    while( iter != m_ProcessMap.end() ) {
        if( nullptr != iter.value() ){
            iter.value()->close();
            //delete iter.value();
           //
            //iter.value()->waitForFinished();
        }
        iter = m_ProcessMap.erase( iter );
    }
    m_ProcessDescs.clear();
    blockSignals(false);
}

void ApplicationsManager::StartApplication(const QString &Name , const QStringList &Arguments, QProcess::OpenMode Mode)
{
    QProcess* newProc =  new QProcess();
    AppDescriptor_t Desc = { Name, Arguments, Mode };
    qDebug() << "App: "<< Name << "Args: " << Arguments;
    newProc->start(Name , Arguments,  Mode );
    m_ProcessMap[nextHndl]   =  newProc;
    m_ProcessDescs[nextHndl] =  Desc;
    newProc->setProperty( "AppHndl", nextHndl );
    connect(newProc,SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(AppFinished(int,QProcess::ExitStatus)) );
    qDebug() << "Process Start: Hndl " << nextHndl;
    newProc->waitForStarted();
    emit ApplicationEvent(nextHndl,APP_STARTED);
    nextHndl++;
}

void ApplicationsManager::AppFinished( int exitCode, QProcess::ExitStatus exitStatus) {

    blockSignals(true);
    QProcess* sender = dynamic_cast<QProcess*>( QObject::sender() );
    if( sender ){
        AppHndl_t apHndl = (AppHndl_t)sender->property("AppHndl").toUInt();
        if( m_ProcessMap.contains( apHndl )){
            m_ProcessMap[apHndl]->deleteLater();
            m_ProcessMap[apHndl] = nullptr;
            qDebug() << "Process Stop: Hndl" << apHndl << " With exitCode "
                     << exitCode << ", exitStatus " << exitStatus;
        }
        else{
            qDebug() << "Can't find process with Hndl " << apHndl;
        }
    }
    blockSignals(false);
}

bool ApplicationsManager::GetAppDescryptor(const AppHndl_t &Hndl , AppDescriptor_t &Desc){
    bool ret = false;
    if( m_ProcessDescs.contains(nextHndl) ){
        Desc = m_ProcessDescs[Hndl];
        ret = true;
    }
    return ret;
}
