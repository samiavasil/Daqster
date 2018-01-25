#include "TemplatePluginObject.h"
#include "QPluginManager.h"
#include "debug.h"
#include <QMainWindow>
#include<QLabel>
#include<QLayout>
#include<QPushButton>

TemplatePluginObject::TemplatePluginObject(QObject *Parent):QBasePluginObject ( Parent  ){
    m_Win = new QMainWindow();
    QLabel* label = new QLabel( );
    label->setText("PluginTemplate Demo");
    m_Win->setCentralWidget(label);
    QPushButton* button = new QPushButton(m_Win);

    m_Win->show();
    m_Win->setAttribute(Qt::WA_DeleteOnClose, true);
    connect( m_Win, SIGNAL(destroyed(QObject*)), this, SLOT(MainWinDestroyed(QObject*)) );
    connect( button, SIGNAL(clicked(bool)), this, SLOT(ShowPlugins()) );
}

TemplatePluginObject::~TemplatePluginObject()
{
    if( m_Win ){
        m_Win->deleteLater();
    }
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

void TemplatePluginObject::ShowPlugins()
{
    Daqster::QPluginManager* pm = Daqster::QPluginManager::instance();
    if( NULL != pm )
    {
        DEBUG << "Plugin Manager: " << pm;
   //     pm->SearchForPlugins();
        pm->ShowPluginManagerGui( m_Win );
    }
}
