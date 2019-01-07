#include "RestApiTesterPluginObject.h"
#include "QPluginManager.h"
#include "debug.h"
#include<MainAppTraderWindow.h>

RestApiTesterPluginObject::RestApiTesterPluginObject(QObject *Parent):QBasePluginObject ( Parent  ),m_Win(NULL){

}

RestApiTesterPluginObject::~RestApiTesterPluginObject()
{
    DeInitialize();
}


void RestApiTesterPluginObject::SetName(const QString &name)
{
    if( m_Win )
    {
        m_Win->setWindowTitle( name );
    }
}

bool RestApiTesterPluginObject::Initialize()
{
    m_Win = new MainAppTraderWindow();
    m_Win->show();
    /* m_Win = new QMainWindow();
    QLabel* label = new QLabel( );
    label->setText("QTCoinTrader Plugin");
    m_Win->setCentralWidget(label);
    QPushButton* button = new QPushButton(m_Win);

    m_Win->show();
    m_Win->setAttribute(Qt::WA_DeleteOnClose, true);
    connect( m_Win, SIGNAL(destroyed(QObject*)), this, SLOT(MainWinDestroyed(QObject*)) );
    connect( button, SIGNAL(clicked(bool)), this, SLOT(ShowPlugins()) );*/
}

void RestApiTesterPluginObject::DeInitialize()
{
    if( m_Win ){
        m_Win->deleteLater();
    }
    DEBUG_V << "TemplatePluginObject destroyed";
}

void RestApiTesterPluginObject::MainWinDestroyed( QObject* obj )
{
    m_Win = NULL;
    deleteLater();
    if( NULL == obj )
        DEBUG << "Strange::!!!";

}

void RestApiTesterPluginObject::ShowPlugins()
{
    Daqster::QPluginManager* pm = Daqster::QPluginManager::instance();
    if( NULL != pm )
    {
        DEBUG << "Plugin Manager: " << pm;
   //     pm->SearchForPlugins();
        pm->ShowPluginManagerGui( m_Win );
    }
}
