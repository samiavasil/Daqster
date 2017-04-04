#include "TemplatePluginObject.h"
#include "QPluginManager.h"
#include "debug.h"
#include <QMainWindow>
#include<QLabel>
#include<QLayout>


TemplatePluginObject::TemplatePluginObject(QObject *Parent):QBasePluginObject ( Parent  ){
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
 //       pm->SearchForPlugins();
        pm->ShowPluginManagerGui();
    }

}

TemplatePluginObject::~TemplatePluginObject()
{
    DEBUG_V << "TemplatePluginObject destroyed";
}

void TemplatePluginObject::SetName(const QString &name)
{
    if( m_Win )
    {
        m_Win->setWindowTitle( name );
    }
}

void TemplatePluginObject::MainWinDestroyed( QObject* obj )
{
    m_Win = NULL;
    deleteLater();
    if( NULL == obj )
        DEBUG << "Strange::!!!";

}
