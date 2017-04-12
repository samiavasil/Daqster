#include "PluginMainObject.h"
#include "QPluginManager.h"
#include "debug.h"
#include <QMainWindow>
#include<QLabel>
#include<QLayout>


PluginFancyObject::PluginFancyObject(QObject *Parent):QBasePluginObject ( Parent  ){
    m_Win = new QMainWindow();
    QLabel* label = new QLabel( );
    label->setText("PluginTemplate Demo");
    m_Win->setCentralWidget(label);
    m_Win->show();
    m_Win->setAttribute(Qt::WA_DeleteOnClose, true);
    connect( m_Win, SIGNAL(destroyed(QObject*)), this, SLOT(MainWinDestroyed(QObject*)) );

    Daqster::QPluginManager* pm = Daqster::QPluginManager::instance();
    if( NULL != pm )
    {
        DEBUG << "Plugin Manager: " << pm;
//        pm->SearchForPlugins();
        pm->ShowPluginManagerGui();
    }

}

PluginFancyObject::~PluginFancyObject()
{
    if( m_Win ){
        m_Win->deleteLater();
    }
    DEBUG_V << "TemplatePluginObject destroyed";
}

void PluginFancyObject::SetName(const QString &name)
{
    if( m_Win )
    {
        m_Win->setWindowTitle( name );
    }
}

void PluginFancyObject::MainWinDestroyed( QObject* obj )
{
    m_Win = NULL;
    deleteLater();
    if( NULL == obj )
        DEBUG << "Strange::!!!";

}
